#pragma once

// struct record {
//   struct record_data data;
//   int next, last;
//   unsigned id;
//   bool valid;
// };

struct hash_node {
  struct hash_node *next;
  void* record;
};
struct hash_node hash_table[1453];

