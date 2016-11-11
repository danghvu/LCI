#ifndef HASHTBL_H_
#define HASHTBL_H_

#include <stdint.h>
#include <stdlib.h>
#include <utility>

using std::pair;
using std::make_pair;

namespace mpiv {

struct Packet;
struct MPIV_Request;

union mpiv_value {
  void* v;
  Packet* packet;
  MPIV_Request* request;
};

class HashTblBase {
 public:
  typedef uint64_t key_type;
  typedef mpiv_value value_type;

  virtual void init() = 0;
  virtual bool insert(const key_type& key, value_type& value) = 0;
};

typedef uint64_t mpiv_key;

constexpr mpiv_key mpiv_make_key(const int& rank, const int& tag) {
  return (((uint64_t)rank << 32) | tag);
}

#include "hashtbl_arr.h"
template <>
struct Config<ConfigType::HASHTBL_ARR> {
  using HashTbl = HashTblArr;
};

#if 0
#include "hashtbl_cock.h"
template <>
struct Config<ConfigType::HASHTBL_COCK> {
  using HasbTbl = HashTblCock;
};
#endif

using HashTbl = Config<HashTblCfg>::HashTbl;

};  // namespace mpiv.
#endif
