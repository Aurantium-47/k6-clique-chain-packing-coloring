// Exact-state search on a NECESSARY endpoint-free relaxation of the K6 target.
// State: two indistinguishable cooldowns per q=2,...,11, sorted within a pair.
// Success selects four currently available copies. Negative conclusions require
// COMPLETE search + an independently verified rank certificate. Limits prove nothing.
#include <bits/stdc++.h>
using namespace std;
constexpr int N=10,H=72;constexpr uint64_t MASK=(uint64_t(1)<<44)-1;
using State=array<uint8_t,20>;using Move=array<uint8_t,10>;
struct Next{State state;Move move;double score;};
vector<uint64_t> table;uint64_t table_mask,used=0,nodes=0,node_limit=2000000,pruned=0,queries=0;
vector<Move> path,bestpath;vector<uint64_t> active_keys;int best=0,cycle_start=-1;
string stop_reason="";double wall_limit=120,pair_penalty=0,elapsed=0;int cap[N][11][H+1];
chrono::steady_clock::time_point start_time;
uint64_t encode(State const&s){uint64_t code=0;for(int i=N-1;i>=0;i--){int q=i+2;code=code*(q*(q+1)/2)+int(s[2*i+1])*(s[2*i+1]+1)/2+s[2*i];}return code;}
uint64_t hash64(uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;x^=x>>31;return x;}
uint64_t slot(uint64_t key){++queries;uint64_t i=hash64(key)&table_mask;while(table[i]&&(table[i]&MASK)!=key)i=(i+1)&table_mask;return i;}
int obstruction(State const&s){for(int h=1;h<=H;h++){int count=0;for(int i=0;i<N;i++)count+=cap[i][s[2*i]][h]+cap[i][s[2*i+1]][h];if(count<4*h)return h;}return 0;}
void gen(State const&s,int q,int left,Move&m,vector<Next>&out,int&capacity_rank){
 if(q==N){if(left)return;State t=s;double score=0;
  for(int i=0;i<N;i++){int chosen=m[i];if(chosen==2)score+=pair_penalty;
   for(int b=0;b<2;b++){int a=2*i+b;if(t[a]==0&&chosen){t[a]=i+1;--chosen;}else t[a]=max(0,int(t[a])-1);score+=double(t[a])/(i+2);}
   if(t[2*i]>t[2*i+1])swap(t[2*i],t[2*i+1]);
  }
  int h=obstruction(t);if(h){++pruned;capacity_rank=max(capacity_rank,h);return;}
  out.push_back({t,m,score});return;
 }
 int available=(s[2*q]==0)+(s[2*q+1]==0);
 for(int n=min(available,left);n>=0;n--){m[q]=n;gen(s,q+1,left-n,m,out,capacity_rank);}
}
// Return positive exact no-extension rank, -1 for a cycle, -2 for a resource limit.
int dfs(State const&s){
 uint64_t key=encode(s)+1;uint64_t index=slot(key);
 if(table[index]){int rank=table[index]>>44;if(rank)return rank;
  auto it=find(active_keys.begin(),active_keys.end(),key);if(it==active_keys.end())throw runtime_error("Invalid active-state table");cycle_start=it-active_keys.begin();return -1;
 }
 ++nodes;if(nodes>node_limit){stop_reason="node_limit";return -2;}
 if((nodes&4095)==0&&chrono::duration<double>(chrono::steady_clock::now()-start_time).count()>wall_limit){stop_reason="wall_time_limit";return -2;}
 if(used*10>=table.size()*8){stop_reason="hash_load_limit";return -2;}
 table[index]=key;++used;
 int depth=path.size();if(depth>best){best=depth;bestpath=path;if(depth%10==0)cerr<<"best "<<best<<" nodes "<<nodes<<" states "<<used<<"\n";}
 active_keys.push_back(key);vector<Next>next;Move m{};int max_rank=0;gen(s,0,4,m,next,max_rank);
 sort(next.begin(),next.end(),[](auto const&a,auto const&b){return a.score<b.score;});
 for(auto const&entry:next){path.push_back(entry.move);int result=dfs(entry.state);if(result<0)return result;path.pop_back();max_rank=max(max_rank,result);}
 active_keys.pop_back();int rank=max_rank+1;if(rank>=(1<<20))throw runtime_error("Rank packing overflow");table[index]=key|(uint64_t(rank)<<44);return rank;
}
int main(int argc,char**argv){
 if(argc>1)node_limit=stoull(argv[1]);if(argc>2)wall_limit=stod(argv[2]);if(argc>3)pair_penalty=stod(argv[3]);int bits=argc>4?stoi(argv[4]):23;
 if(bits<10||bits>29)throw runtime_error("Unsupported hash size");table.resize(uint64_t(1)<<bits);table_mask=table.size()-1;
 for(int q=0;q<N;q++)for(int r=0;r<q+2;r++)for(int h=1;h<=H;h++)cap[q][r][h]=h<=r?0:1+(h-1-r)/(q+2);
 start_time=chrono::steady_clock::now();State root{};int result=dfs(root);elapsed=chrono::duration<double>(chrono::steady_clock::now()-start_time).count();if(result!=-1)path=bestpath;
 string certificate="";if(result>0&&argc>5){certificate=argv[5];vector<uint64_t> records;records.reserve(used);for(auto e:table)if(e)records.push_back(e);sort(records.begin(),records.end(),[](auto x,auto y){return (x&MASK)<(y&MASK);});ofstream f(certificate,ios::binary);for(auto e:records)for(int b=0;b<8;b++)f.put(char((e>>(8*b))&255));if(!f)throw runtime_error("Certificate write failed");}
 cout<<"{\"necessary_relaxation_only\":true,\"q_values\":[2,3,4,5,6,7,8,9,10,11],\"copies_per_q\":2,\"demand\":4,\"future_horizon\":72,\"nodes\":"<<nodes<<",\"stored_states\":"<<used<<",\"pruned_moves\":"<<pruned<<",\"hash_queries\":"<<queries<<",\"hash_slots\":"<<table.size()<<",\"node_limit\":"<<node_limit<<",\"wall_limit\":"<<wall_limit<<",\"elapsed\":"<<setprecision(12)<<elapsed<<",\"pair_penalty\":"<<pair_penalty<<",\"status\":\""<<(result==-1?"CYCLE_FOUND":result==-2?"RESOURCE_LIMIT":"COMPLETE_NO_CYCLE")<<"\",\"stop_reason\":\""<<stop_reason<<"\",\"root_rank\":"<<(result>0?result:0)<<",\"certificate\":\""<<certificate<<"\",\"cycle_start\":"<<cycle_start<<",\"rows_q2_to_q11\":[";
 for(int j=0;j<(int)path.size();j++){if(j)cout<<",";cout<<"[";for(int q=0;q<N;q++){if(q)cout<<",";cout<<int(path[j][q]);}cout<<"]";}cout<<"]}\n";
 cerr<<"DONE nodes "<<nodes<<" best "<<best<<" result "<<result<<" stopped "<<stop_reason<<" elapsed "<<elapsed<<"\n";
}
