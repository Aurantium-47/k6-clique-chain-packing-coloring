// Independent checker for the binary cooldown-rank certificate.
// No search routine, Z3, floating point, assertions, or search hash table is used.
// Every set of FOUR LABELED available copies is covered (including choices that
// canonicalize to the same state). Future capacities are independently built by
// enumerating event times. A uniform-capacity shortcut covers ALL selections.
// Usage: verify_full_rank CERTIFICATE.bin [--first-high]
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
// Review-only portability helpers; mathematical verification is unchanged.
static std::uint64_t review_bswap64(std::uint64_t value) {
 std::uint64_t result=0;for(int i=0;i<8;++i){result=(result<<8)|(value&255);value>>=8;}return result;
}
static int review_popcount(std::uint32_t value) {
 int count=0;while(value){value&=value-1;++count;}return count;
}
using namespace std;
constexpr uint64_t KEY_MASK=(uint64_t(1)<<44)-1;
constexpr int HORIZON=72;
using State=array<int,20>;
struct Test {uint32_t mask;int threshold;int horizon;};
static uint64_t total_states(){uint64_t x=1;for(int q=2;q<=11;q++)x*=q*(q+1)/2;return x;}
static State decode(uint64_t id){State s{};for(int q=2;q<=11;q++){int radix=q*(q+1)/2,digit=id%radix;id/=radix;int b=0;while((b+1)*(b+2)/2<=digit)++b;int a=digit-b*(b+1)/2;if(!(0<=a&&a<=b&&b<q))throw runtime_error("Invalid state digit");s[2*(q-2)]=a;s[2*(q-2)+1]=b;}if(id)throw runtime_error("State ID too large");return s;}
static uint64_t encode(State const&s){uint64_t id=0,multiplier=1;for(int q=2;q<=11;q++){int a=s[2*(q-2)],b=s[2*(q-2)+1];if(a>b)swap(a,b);id+=multiplier*(b*(b+1)/2+a);multiplier*=q*(q+1)/2;}return id;}
static uint64_t choose4(int n){return n<4?0:uint64_t(n)*(n-1)*(n-2)*(n-3)/24;}
struct RankLookup {
 vector<uint64_t> slots;unsigned bits;uint64_t mask;
 explicit RankLookup(size_t count){bits=1;while((uint64_t(1)<<bits)<2*count)++bits;slots.resize(uint64_t(1)<<bits);mask=slots.size()-1;}
 uint64_t position(uint64_t key)const{return (key*uint64_t(0x9e3779b97f4a7c15ULL))>>(64-bits);}
 void add(uint64_t entry){uint64_t key=entry&KEY_MASK,p=position(key);while(slots[p]){if((slots[p]&KEY_MASK)==key)throw runtime_error("Duplicate state ID");p=(p+1)&mask;}slots[p]=entry;}
 int rank(uint64_t key)const{uint64_t p=position(key);while(slots[p]){if((slots[p]&KEY_MASK)==key)return slots[p]>>44;p=(p+1)&mask;}return 0;}
};
int main(int argc,char**argv){
 try{
  if(argc<2||argc>3)throw runtime_error("Usage: verifier certificate.bin [--first-high]");
  bool first_high=argc==3;if(first_high&&string(argv[2])!="--first-high")throw runtime_error("Unknown option");
  auto start=chrono::steady_clock::now();ifstream file(argv[1],ios::binary|ios::ate);if(!file)throw runtime_error("Cannot open certificate");auto bytes=file.tellg();if(bytes<=0||uint64_t(bytes)%8)throw runtime_error("Invalid byte count");size_t count=uint64_t(bytes)/8;file.seekg(0);vector<uint64_t> entries(count);
  file.read(reinterpret_cast<char*>(entries.data()),bytes);if(!file)throw runtime_error("Truncated certificate");uint16_t endian=1;if(*reinterpret_cast<unsigned char*>(&endian)!=1)for(auto&e:entries)e=review_bswap64(e);
  RankLookup lookup(count);uint64_t previous=0;int max_rank=0;
  for(uint64_t entry:entries){uint64_t key=entry&KEY_MASK;int rank=entry>>44;if(key<=previous||key>total_states()||rank<1)throw runtime_error("Unsorted, out-of-range, or unfinished record");previous=key;max_rank=max(max_rank,rank);State s=decode(key-1);if(encode(s)!=key-1)throw runtime_error("State coding round-trip failed");lookup.add(entry);}
  int root_rank=lookup.rank(1);if(!root_rank)throw runtime_error("Initial all-zero state is missing");
  // Independent event enumeration for base cooldown max(r-1,0), not the
  // closed-form capacity formula used by the certificate generator.
  array<array<array<uint8_t,HORIZON+1>,11>,10> capacity{};
  array<uint32_t,HORIZON+1> divisor_mask{};
  for(int q=2;q<=11;q++)for(int r=0;r<q;r++){
   int base=max(r-1,0);for(int horizon=1;horizon<=HORIZON;horizon++){
    int occurrences=0;for(int time=base;time<horizon;time+=q)occurrences++;
    capacity[q-2][r][horizon]=occurrences;
   }
  }
  for(int h=1;h<=HORIZON;h++)for(int i=0;i<20;i++)if(h%(i/2+2)==0)divisor_mask[h]|=uint32_t(1)<<i;
  uint64_t checked=0,all_choices=0,uniform_choices=0,enumerated_choices=0,capacity_choices=0,rank_choices=0,uniform_states=0;
  int largest_horizon=0;
  for(uint64_t entry:entries){
   uint64_t key=entry&KEY_MASK;int rank=entry>>44;State s=decode(key-1),base{};vector<int>available;uint32_t available_mask=0;int high_available=0;
   for(int i=0;i<20;i++){base[i]=max(s[i]-1,0);if(s[i]==0){available.push_back(i);available_mask|=uint32_t(1)<<i;if(i>=18)high_available++;}}
   bool restricted_root=first_high&&key==1;uint64_t choices=choose4(available.size());if(restricted_root)choices-=choose4(int(available.size())-high_available);all_choices+=choices;
   vector<Test>tests;bool uniformly_blocked=false;int max_h=min(HORIZON,rank-1);
   for(int h=1;h<=max_h;h++){
    int cap=0;for(int i=0;i<20;i++)cap+=capacity[i/2][s[i]][h];
    int threshold=4-(cap-4*h);if(threshold<=0)continue;
    uint32_t mask=available_mask&divisor_mask[h];
    // Selecting an available copy replaces cooldown 0 by q-1, reducing
    // the h-step capacity by exactly 1 when q does not divide h.
    if(review_popcount(mask)<threshold){uniformly_blocked=true;largest_horizon=max(largest_horizon,h);break;}
    bool merged=false;for(auto&t:tests)if(t.mask==mask){if(threshold>t.threshold)t={mask,threshold,h};merged=true;break;}if(!merged)tests.push_back({mask,threshold,h});
   }
   if(uniformly_blocked){uniform_states++;uniform_choices+=choices;checked++;continue;}
   int n=available.size();
   for(int a=0;a<n;a++)for(int b=a+1;b<n;b++)for(int c=b+1;c<n;c++)for(int d=c+1;d<n;d++){
    int ids[4]={available[a],available[b],available[c],available[d]};uint32_t selected=0;for(int i:ids)selected|=uint32_t(1)<<i;
    if(restricted_root&&(selected&(uint32_t(3)<<18))==0)continue;
    enumerated_choices++;bool blocked=false;
    for(auto const&t:tests)if(review_popcount(selected&t.mask)<t.threshold){blocked=true;capacity_choices++;largest_horizon=max(largest_horizon,t.horizon);break;}
    if(blocked)continue;
    State next=base;for(int i:ids)next[i]=i/2+1;uint64_t nextkey=encode(next)+1;int next_rank=lookup.rank(nextkey);
    if(next_rank<1||next_rank>=rank){cerr<<"FAIL state_id="<<key-1<<" rank="<<rank<<" selected="<<selected<<" successor="<<nextkey-1<<" successor_rank="<<next_rank<<"\n";throw runtime_error("Uncovered or nondecreasing successor");}
    rank_choices++;
   }
   checked++;
   if((checked%1000000)==0)cerr<<"checked "<<checked<<" / "<<count<<" states\n";
  }
  if(checked!=count||all_choices!=uniform_choices+enumerated_choices||enumerated_choices!=capacity_choices+rank_choices)throw runtime_error("Incomplete accounting");
  double elapsed=chrono::duration<double>(chrono::steady_clock::now()-start).count();
  cout<<"{\"status\":\"PASS\",\"first_step_contains_q11\":"<<(first_high?"true":"false")<<",\"root_rank\":"<<root_rank<<",\"max_rank\":"<<max_rank<<",\"verified_states\":"<<checked<<",\"covered_labeled_transition_choices\":"<<all_choices<<",\"uniform_capacity_states\":"<<uniform_states<<",\"uniform_capacity_choices\":"<<uniform_choices<<",\"explicitly_enumerated_choices\":"<<enumerated_choices<<",\"individual_capacity_choices\":"<<capacity_choices<<",\"decreasing_rank_choices\":"<<rank_choices<<",\"largest_capacity_horizon_used\":"<<largest_horizon<<",\"certificate_bytes\":"<<uint64_t(bytes)<<",\"lookup_slots\":"<<lookup.slots.size()<<",\"seconds\":"<<elapsed<<"}\n";
 }catch(exception const&e){cerr<<"VERIFICATION FAILURE: "<<e.what()<<"\n";return 1;}
}
