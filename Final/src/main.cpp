#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#define uint uint32_t
#define ulong uint64_t
#define P10(x) ((x << 3) + (x << 1))

#ifdef LOCAL
#define TRAIN "../data/18908526/test_data.txt"
#define RESULT "../data/std/result.txt"
#else
#define TRAIN "/data/test_data.txt"
#define RESULT "/projects/student/result.txt"
#endif

namespace Color {
inline void reset() { fprintf(stderr, "\033[0m"); }
inline void red() { fprintf(stderr, "\033[1;31m"); }
inline void green() { fprintf(stderr, "\033[1;32m"); }
inline void yellow() { fprintf(stderr, "\033[1;33m"); }
inline void blue() { fprintf(stderr, "\033[1;34m"); }
inline void magenta() { fprintf(stderr, "\033[1;35m"); }
inline void cyan() { fprintf(stderr, "\033[1;36m"); }
inline void orange() { fprintf(stderr, "\033[38;5;214m"); }
inline void newline() { fprintf(stderr, "\n"); }
}  // namespace Color
class ScopeTime {
 public:
  ScopeTime() : m_begin(std::chrono::high_resolution_clock::now()) {}
  inline double elapsed() const {
    auto t = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - m_begin);
    double elapsed = (double)(t.count() * 1.0) / 1000.0;
    return elapsed;
  }

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> m_begin;
};

/*
 * 常量定义
 */
const uint MAX_EDGE = 3000000 + 7;  // 最多边数目
const uint MAX_NODE = 6000000 + 7;  // 最多点数目
const uint T = 4;                   // 线程个数

/************************** LoadData Begin ****************************/
struct HashTable {
  /*
   * 开源HashTable
   */
  static const int MOD1 = 6893911;
  static const int MOD2 = 5170427;
  struct Data {
    uint key;
    int val = -1;
  };
  uint count = 0;
  Data Map[MOD1];
  uint HashIdx[MOD1];
  uint Size() { return count; }
  uint hash(const uint &k, const int &i) {
    return (k % MOD1 + i * (MOD2 - k % MOD2)) % MOD1;
  }
  void Insert(const uint &key) {
    for (int i = 0; i < MOD1; ++i) {
      const uint &val = this->hash(key, i);
      if (Map[val].val == -1) {
        Map[val].key = key;
        Map[val].val = count;
        HashIdx[count++] = val;
        break;
      }
      if (Map[val].key == key) break;
    }
  }
  int Query(const uint &key) {
    for (int i = 0; i < MOD1; ++i) {
      const uint &val = hash(key, i);
      if (Map[val].val == -1) return -1;
      if (Map[val].key == key) return Map[val].val;
    }
    return -1;
  }
};
struct DFSEdge {
  uint idx;
  ulong w;
};
struct ReadEdge {
  uint u, v;
  ulong w;
};
struct LoadInfo {
  uint offsz = 0;                     // 线程hashmap偏移
  HashTable hashmap;                  // 线程独立hashmap
  std::vector<uint> ids;              // u % T = pid 的所有id
  std::vector<uint> th_ids[T];        // u % T的id
  std::vector<ReadEdge> edges;        // u % T = pid 的所有边
  std::vector<ReadEdge> th_edges[T];  // u % T 的边
};
uint g_NodeNum = 0;                      // 加载完数据后结点数目
uint g_EdgeNum = 0;                      // 加载完数据后边数目
uint Head[MAX_NODE], HeadLen[MAX_NODE];  // 前向星标记位置和长度
uint Back[MAX_NODE], BackLen[MAX_NODE];  // 后向星标记位置和长度
ReadEdge Edges[MAX_EDGE];                // 多线程边合并后的所有边
DFSEdge GHead[MAX_NODE];                 // 前向星所有边
DFSEdge GBack[MAX_NODE];                 // 后向星所有边
LoadInfo LoadInfos[T];                   // 多线程加载数据

/*
 * 加载数据
 * 1. 多线程解析buffer, 每个线程存 u%T 的边
 * 2. 将每个线程th_edges[u%T]的边memcpy到LoadInfo[pid]里面,并排序
 * 3. LoadInfo[pid]里面的边进行Hash
 * 4. 计算每个线程HashTable的偏移量
 * 5. 多线程对每个线程的HashTable重新Hash
 * 6. 合并HashTable
 * 7. 根据真实ID大小重新对HashTable的val进行排名, Rank
 * 8. 遍历所有边,将u, v改成Hash之后的映射id
 * 9. 构造前向星
 * 10. 构造后向星
 */
void InitLoadInfo() {
  for (uint i = 0; i < T; ++i) {
    LoadInfos[i].ids.reserve(MAX_NODE);
    LoadInfos[i].edges.reserve(MAX_EDGE);
    for (uint j = 0; j < T; ++j) {
      LoadInfos[i].th_ids[j].reserve(MAX_NODE);
      LoadInfos[i].th_edges[j].reserve(MAX_EDGE);
    }
  }
};
void addEdge(const uint &u, const uint &v, const ulong &w, LoadInfo &data) {
  uint umod = u % T, vmod = v % T;
  data.th_edges[umod].emplace_back(ReadEdge{u, v, w});
  data.th_ids[umod].emplace_back(u);
  data.th_ids[vmod].emplace_back(v);
}
void HandleReadBuffer(const char *buffer, uint st, uint ed, uint pid) {
  const char *ptr = buffer + st, *end = buffer + ed;
  uint u = 0, v = 0, w = 0;
  auto &loadinfo = LoadInfos[pid];
  while (ptr < end) {
    while (*ptr != ',') {
      u = P10(u) + *ptr - '0';
      ++ptr;
    }
    ++ptr;
    while (*ptr != ',') {
      v = P10(v) + *ptr - '0';
      ++ptr;
    }
    ++ptr;
    while (*ptr != '\r' && *ptr != '\n') {
      w = P10(w) + *ptr - '0';
      ++ptr;
    }
    if (*ptr == '\r') ++ptr;
    ++ptr;
    addEdge(u, v, w, loadinfo);
    u = v = w = 0;
  }
}
void ReadBuffer() {
  uint fd = open(TRAIN, O_RDONLY);
  uint bufsize = lseek(fd, 0, SEEK_END);
  char *buffer = (char *)mmap(NULL, bufsize, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  std::thread Th[T];
  uint st = 0, block = bufsize / T;
  for (uint i = 0; i < T; ++i) {
    if (i == T - 1) {
      Th[i] = std::thread(HandleReadBuffer, buffer, st, bufsize, i);
      break;
    }
    uint ed = st + block;
    while (buffer[ed] != '\n') ++ed;
    ++ed;
    Th[i] = std::thread(HandleReadBuffer, buffer, st, ed, i);
    st = ed;
  }
  for (auto &it : Th) it.join();
}
void MergeSortIdAndEdge(uint pid) {
  auto &data = LoadInfos[pid];
  for (uint i = 0; i < T; ++i) {
    const auto &info = LoadInfos[i];
    data.edges.insert(data.edges.end(), info.th_edges[pid].begin(),
                      info.th_edges[pid].end());
    data.ids.insert(data.ids.end(), info.th_ids[pid].begin(),
                    info.th_ids[pid].end());
  }
  std::sort(data.ids.begin(), data.ids.end());
  std::sort(data.edges.begin(), data.edges.end(),
            [&](const ReadEdge &e1, const ReadEdge &e2) {
              if (e1.u == e2.u) return e1.v < e2.v;
              return e1.u < e2.u;
            });
}
void HashIds(uint pid) {
  auto &data = LoadInfos[pid];
  if (data.ids.empty()) return;
  uint pre = data.ids[0] + 7;
  for (auto &v : data.ids) {
    if (v == pre) continue;
    pre = v;
    data.hashmap.Insert(v);
  }
}
std::vector<uint> IDDom;
void CalMapOffSet() {
  for (uint i = 1; i < T; ++i) {
    const uint &sz = LoadInfos[i - 1].hashmap.Size();
    LoadInfos[i].offsz = LoadInfos[i - 1].offsz + sz;
  }
  g_NodeNum = LoadInfos[T - 1].offsz + LoadInfos[T - 1].hashmap.Size();
  IDDom.reserve(g_NodeNum);
}
void MergeHashTable(uint pid) {
  auto &data = LoadInfos[pid];
  uint offsz = LoadInfos[pid].offsz;
  for (uint i = 0; i < data.hashmap.Size(); ++i) {
    auto &p = data.hashmap.Map[data.hashmap.HashIdx[i]];
    p.val += offsz;
    IDDom[p.val] = p.key;
  }
}
std::vector<uint> Rank;
void RankHashTable() {
  std::vector<uint> vec(g_NodeNum);
  Rank.reserve(g_NodeNum);
  for (uint i = 0; i < g_NodeNum; ++i) vec[i] = i;
  std::sort(vec.begin(), vec.end(),
            [&](const uint &x, const uint &y) { return IDDom[x] < IDDom[y]; });
  for (uint i = 0; i < g_NodeNum; ++i) {
    const uint &x = vec[i];
    Rank[x] = i;
  }
}
void ReHashTable(uint pid) {
  auto &hashmap = LoadInfos[pid].hashmap;
  for (uint i = 0; i < hashmap.Size(); ++i) {
    auto &p = hashmap.Map[hashmap.HashIdx[i]];
    p.val = Rank[p.val];
  }
}
void RankEdge(uint pid) {
  auto &data = LoadInfos[pid];
  for (auto &e : data.edges) {
    e.u = LoadInfos[e.u % T].hashmap.Query(e.u);
    e.v = LoadInfos[e.v % T].hashmap.Query(e.v);
  }
}
void MergeEdge() {
  uint left[T] = {0};
  ReadEdge *ptr = Edges;
  while (true) {
    uint minx = g_NodeNum + 7, minidx = -1;
    for (uint i = 0; i < T; ++i) {
      const auto &data = LoadInfos[i];
      if (left[i] < data.edges.size() && data.edges[left[i]].u < minx) {
        minx = data.edges[left[i]].u;
        minidx = i;
      }
    }
    if (minidx == -1) break;
    const auto &data = LoadInfos[minidx];
    uint &l = left[minidx];
    while (data.edges[l].u == minx) {
      memcpy(ptr, &data.edges[left[minidx]], sizeof(ReadEdge));
      ++ptr;
      ++l;
    }
  }
  g_EdgeNum = ptr - Edges;
}
void BuildGraphHead(uint pid) {
  uint pre = g_NodeNum + 7;
  for (uint i = 0; i < g_EdgeNum; ++i) {
    const auto &e = Edges[i];
    if (e.u % T == pid) {
      if (e.u != pre) {
        Head[e.u] = i;
      }
      ++HeadLen[e.u];
      pre = e.u;
      GHead[i].idx = e.v;
      GHead[i].w = e.w;
    }
    if (e.v % T == pid) {
      ++BackLen[e.v];
    }
  }
}
void BuildGraphBack(uint pid) {
  std::vector<uint> cnt(g_NodeNum, 0);
  for (int i = g_EdgeNum - 1; i >= 0; --i) {
    const auto &e = Edges[i];
    if (e.v % T == pid) {
      auto &p = GBack[Back[e.v] + cnt[e.v]];
      p.idx = e.u;
      p.w = e.w;
      ++cnt[e.v];
    }
  }
}
void LoadData() {
#ifdef LOCAL
  ScopeTime t;
#endif
  InitLoadInfo();
  ReadBuffer();
  std::thread Th[T];
  for (uint i = 0; i < T; ++i) Th[i] = std::thread(MergeSortIdAndEdge, i);
  for (auto &it : Th) it.join();
  for (uint i = 0; i < T; ++i) Th[i] = std::thread(HashIds, i);
  for (auto &it : Th) it.join();
  CalMapOffSet();
  for (uint i = 0; i < T; ++i) Th[i] = std::thread(MergeHashTable, i);
  for (auto &it : Th) it.join();
  RankHashTable();
  for (uint i = 0; i < T; ++i) Th[i] = std::thread(ReHashTable, i);
  for (auto &it : Th) it.join();
  for (uint i = 0; i < T; ++i) Th[i] = std::thread(RankEdge, i);
  for (auto &it : Th) it.join();
  MergeEdge();
  for (uint i = 0; i < T; ++i) Th[i] = std::thread(BuildGraphHead, i);
  for (auto &it : Th) it.join();
  for (uint i = 1; i <= g_NodeNum; ++i) {
    Head[i] = Head[i - 1] + HeadLen[i - 1];
    Back[i] = Back[i - 1] + BackLen[i - 1];
  }
  for (uint i = 0; i < T; ++i) Th[i] = std::thread(BuildGraphBack, i);
  for (auto &it : Th) it.join();
#ifdef LOCAL
  Color::green();
  printf("@ LoadData: [结点: %d, 边: %d] [cost: %.4fs]\n", g_NodeNum, g_EdgeNum,
         t.elapsed());
  Color::reset();
#endif
}
/************************** LoadData End ****************************/
int main() {
  LoadData();
  return 0;
}