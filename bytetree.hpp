#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "bitboard.hpp"

enum ScaleRegType {
  REG_DEV_MINORS = 0,
  REG_BISHOP_MOBILITY = 1,
  REG_KNIGHT_MOBILITY = 2,
  REG_CENTER_OCC = 3,
  REG_PASSED_PAWNS = 4,
  REG_PHALANX_FILES = 5,
  REG_KING_SHIELD = 6,
  REG_OPEN_ROOKS = 7,
  REG_KING_CENTRALITY = 8,
  REG_SEVENTH_INVASION = 9,
  SCALE_REG_COUNT = 10
};

const char* SCALE_REG_NAMES[] = {"DEV_MINORS",  "BISHOP_MOB",   "KNIGHT_MOB",
                                 "CENTER_OCC",  "PASSED_PAWNS", "PHALANX_FILES",
                                 "KING_SHIELD", "OPEN_ROOKS",   "KING_CENTRAL",
                                 "SEVENTH_RANK"};

struct SideByteRegisters {
  uint8_t pawn_files_byte = 0;
  uint8_t passed_files_byte = 0;
  uint8_t phalanx_files_byte = 0;
  uint8_t isolated_files_byte = 0;
  uint8_t open_rook_files_byte = 0;
  uint8_t center_ranks_byte = 0;
  uint8_t seventh_rank_byte = 0;
  uint8_t king_shield_byte = 0;
  uint8_t status_flags_byte = 0;
  int8_t counts[SCALE_REG_COUNT] = {0};
};

inline SideByteRegisters extract_side_bytes(const ChessBoard& b,
                                            bool for_white) {
  SideByteRegisters reg;
  int us = for_white ? 0 : 6;
  int them = for_white ? 6 : 0;
  uint64_t all = b.all_occ();
  uint64_t us_occ = for_white ? b.white_occ() : b.black_occ();

  uint64_t us_pawns = b.bb[us + 0];
  uint64_t th_pawns = b.bb[them + 0];

  int start_pawn_rank = for_white ? 1 : 6;
  uint64_t advanced_pawns = us_pawns & ~ATTACKS.rank_masks[start_pawn_rank];
  uint8_t adv_pawn_files = project_to_file_byte(advanced_pawns);

  reg.pawn_files_byte = project_to_file_byte(us_pawns);
  uint8_t enemy_pawn_files = project_to_file_byte(th_pawns);
  uint8_t enemy_file_span = enemy_pawn_files |
                            static_cast<uint8_t>(enemy_pawn_files << 1) |
                            (enemy_pawn_files >> 1);

  reg.passed_files_byte = reg.pawn_files_byte & ~enemy_file_span;
  reg.phalanx_files_byte =
      adv_pawn_files & (static_cast<uint8_t>(reg.pawn_files_byte << 1) |
                        (reg.pawn_files_byte >> 1));
  reg.isolated_files_byte =
      reg.pawn_files_byte & ~(static_cast<uint8_t>(reg.pawn_files_byte << 1) |
                              (reg.pawn_files_byte >> 1));

  uint8_t rook_files = project_to_file_byte(b.bb[us + 3]);
  reg.open_rook_files_byte = rook_files & ~reg.pawn_files_byte;

  uint64_t us_minors_pawns = b.bb[us + 0] | b.bb[us + 1] | b.bb[us + 2];
  reg.center_ranks_byte = (extract_rank_byte(us_minors_pawns, 3) |
                           extract_rank_byte(us_minors_pawns, 4)) &
                          0b00111100;

  int sev_r = for_white ? 6 : 1;
  reg.seventh_rank_byte =
      extract_rank_byte(b.bb[us + 0] | b.bb[us + 3] | b.bb[us + 4], sev_r);

  int back_r = for_white ? 0 : 7;
  uint8_t back_minors_byte =
      extract_rank_byte(b.bb[us + 1] | b.bb[us + 2], back_r);
  int total_minors = __builtin_popcountll(b.bb[us + 1] | b.bb[us + 2]);
  reg.counts[REG_DEV_MINORS] = static_cast<int8_t>(
      std::max(0, total_minors - __builtin_popcount(back_minors_byte)));

  int b_mob = 0;
  uint64_t bishops = b.bb[us + 2];
  while (bishops) {
    int sq = __builtin_ctzll(bishops);
    bishops &= bishops - 1;
    b_mob += __builtin_popcountll(slide_attacks(sq, all, false) & ~us_occ);
  }
  reg.counts[REG_BISHOP_MOBILITY] = static_cast<int8_t>(std::min(28, b_mob));

  int n_mob = 0;
  uint64_t knights = b.bb[us + 1];
  while (knights) {
    int sq = __builtin_ctzll(knights);
    knights &= knights - 1;
    n_mob += __builtin_popcountll(ATTACKS.knight_attacks[sq] & ~us_occ);
  }
  reg.counts[REG_KNIGHT_MOBILITY] = static_cast<int8_t>(std::min(20, n_mob));

  reg.counts[REG_CENTER_OCC] =
      static_cast<int8_t>(__builtin_popcount(reg.center_ranks_byte));
  reg.counts[REG_PASSED_PAWNS] =
      static_cast<int8_t>(__builtin_popcount(reg.passed_files_byte));
  reg.counts[REG_PHALANX_FILES] =
      static_cast<int8_t>(__builtin_popcount(reg.phalanx_files_byte));
  reg.counts[REG_OPEN_ROOKS] =
      static_cast<int8_t>(__builtin_popcount(reg.open_rook_files_byte));
  reg.counts[REG_SEVENTH_INVASION] =
      static_cast<int8_t>(__builtin_popcount(reg.seventh_rank_byte));

  bool can_castle_ks =
      for_white ? ((b.castling & 1) != 0) : ((b.castling & 4) != 0);
  bool can_castle_qs =
      for_white ? ((b.castling & 2) != 0) : ((b.castling & 8) != 0);

  if (b.bb[us + 5]) {
    int ksq = __builtin_ctzll(b.bb[us + 5]);
    int kr = ksq / 8, kc = ksq % 8;
    uint8_t k_file_mask =
        static_cast<uint8_t>((1 << kc) | (kc > 0 ? (1 << (kc - 1)) : 0) |
                             (kc < 7 ? (1 << (kc + 1)) : 0));
    int shield_r = for_white ? std::min(7, kr + 1) : std::max(0, kr - 1);
    reg.king_shield_byte = (extract_rank_byte(us_pawns, kr) |
                            extract_rank_byte(us_pawns, shield_r)) &
                           k_file_mask;
    reg.counts[REG_KING_SHIELD] =
        static_cast<int8_t>(__builtin_popcount(reg.king_shield_byte));

    int center_dist = std::abs(kr - 3) + std::abs(kc - 3);
    reg.counts[REG_KING_CENTRALITY] =
        static_cast<int8_t>(std::max(0, 7 - center_dist));

    if (kr == back_r && (kc >= 5 || kc <= 2)) reg.status_flags_byte |= (1 << 2);
    if (center_dist <= 2) reg.status_flags_byte |= (1 << 5);
  }

  uint8_t back_occ_byte = extract_rank_byte(all, back_r);
  if (can_castle_ks && !(back_occ_byte & (1 << 5)))
    reg.status_flags_byte |= (1 << 0);
  if ((can_castle_ks && !(back_occ_byte & (1 << 6))) ||
      (can_castle_qs && !(back_occ_byte & 0b00001110))) {
    reg.status_flags_byte |= (1 << 1);
  }
  if (__builtin_popcountll(b.bb[us + 2]) >= 2)
    reg.status_flags_byte |= (1 << 3);

  if (b.bb[them + 5]) {
    int eksq = __builtin_ctzll(b.bb[them + 5]);
    if (__builtin_popcountll(ATTACKS.king_attacks[eksq] & b.bb[them + 0]) <=
        1) {
      reg.status_flags_byte |= (1 << 4);
    }
    if (b.bb[us + 5]) {
      int ksq = __builtin_ctzll(b.bb[us + 5]);
      int dist =
          std::abs((ksq / 8) - (eksq / 8)) + std::abs((ksq % 8) - (eksq % 8));
      if (dist <= 3) reg.status_flags_byte |= (1 << 6);
    }
  }

  uint8_t center_knights = extract_rank_byte(b.bb[us + 1], for_white ? 3 : 4) |
                           extract_rank_byte(b.bb[us + 1], for_white ? 4 : 3);
  if (center_knights & 0b00111100) reg.status_flags_byte |= (1 << 7);

  return reg;
}

enum BytePredicate {
  BYTE_CENTER_FILES_OCC = 0,
  BYTE_BOTH_D_E_FILES = 1,
  BYTE_PASSED_FILE_EXISTS = 2,
  BYTE_PHALANX_IN_CENTER = 3,
  BYTE_NO_ISOLATED_FILES = 4,
  BYTE_ROOK_OPEN_FILE = 5,
  BYTE_CENTER_RANK_2PLUS = 6,
  BYTE_DEV_MINORS_2PLUS = 7,
  BYTE_DEV_MINORS_3PLUS = 8,
  BYTE_BISHOP_MOB_7PLUS = 9,
  BYTE_KING_SHIELD_2PLUS = 10,
  BYTE_CASTLE_READY_OR_DONE = 11,
  BYTE_BISHOP_PAIR_FLAG = 12,
  BYTE_ENEMY_KING_EXPOSED = 13,
  BYTE_ENDGAME_KING_CENTRAL = 14,
  BYTE_SEVENTH_RANK_ACTIVE = 15,
  BYTE_PRED_COUNT = 16
};

const char* BYTE_PRED_NAMES[] = {
    "BYTE_CENTER_FILES_OCC",     "BYTE_BOTH_D_E_FILES",
    "BYTE_PASSED_FILE_EXISTS",   "BYTE_PHALANX_IN_CENTER",
    "BYTE_NO_ISOLATED_FILES",    "BYTE_ROOK_OPEN_FILE",
    "BYTE_CENTER_RANK_2PLUS",    "BYTE_DEV_MINORS_2PLUS",
    "BYTE_DEV_MINORS_3PLUS",     "BYTE_BISHOP_MOB_7PLUS",
    "BYTE_KING_SHIELD_2PLUS",    "BYTE_CASTLE_READY_OR_DONE",
    "BYTE_BISHOP_PAIR_FLAG",     "BYTE_ENEMY_KING_EXPOSED",
    "BYTE_ENDGAME_KING_CENTRAL", "BYTE_SEVENTH_RANK_ACTIVE"};

inline bool evaluate_byte_predicate(BytePredicate p,
                                    const SideByteRegisters& r) {
  switch (p) {
    case BYTE_CENTER_FILES_OCC:
      return (r.center_ranks_byte & 0b00011000) != 0;
    case BYTE_BOTH_D_E_FILES:
      return (r.center_ranks_byte & 0b00011000) == 0b00011000;
    case BYTE_PASSED_FILE_EXISTS:
      return r.passed_files_byte != 0;
    case BYTE_PHALANX_IN_CENTER:
      return (r.phalanx_files_byte & 0b00111100) != 0;
    case BYTE_NO_ISOLATED_FILES:
      return r.isolated_files_byte == 0;
    case BYTE_ROOK_OPEN_FILE:
      return r.open_rook_files_byte != 0;
    case BYTE_CENTER_RANK_2PLUS:
      return r.counts[REG_CENTER_OCC] >= 2;
    case BYTE_DEV_MINORS_2PLUS:
      return r.counts[REG_DEV_MINORS] >= 2;
    case BYTE_DEV_MINORS_3PLUS:
      return r.counts[REG_DEV_MINORS] >= 3;
    case BYTE_BISHOP_MOB_7PLUS:
      return r.counts[REG_BISHOP_MOBILITY] >= 7;
    case BYTE_KING_SHIELD_2PLUS:
      return r.counts[REG_KING_SHIELD] >= 2;
    case BYTE_CASTLE_READY_OR_DONE:
      return ((r.status_flags_byte & 0b00000011) == 0b00000011) ||
             ((r.status_flags_byte >> 2) & 1);
    case BYTE_BISHOP_PAIR_FLAG:
      return ((r.status_flags_byte >> 3) & 1) != 0;
    case BYTE_ENEMY_KING_EXPOSED:
      return ((r.status_flags_byte >> 4) & 1) != 0;
    case BYTE_ENDGAME_KING_CENTRAL:
      return ((r.status_flags_byte >> 5) & 1) != 0;
    case BYTE_SEVENTH_RANK_ACTIVE:
      return r.seventh_rank_byte != 0;
    default:
      return false;
  }
}

struct ByteNode {
  bool is_leaf = true;
  BytePredicate predicate = BYTE_CENTER_FILES_OCC;
  int16_t base_weight = 0;
  ScaleRegType scale_reg = REG_DEV_MINORS;
  int16_t scale_weight = 0;
  int yes_idx = -1;
  int no_idx = -1;
};

struct SymbolicByteTree {
  std::vector<ByteNode> nodes;

  int32_t evaluate_side(const SideByteRegisters& r, int idx = 0) const {
    if (nodes.empty() || idx < 0 || idx >= (int)nodes.size()) return 0;
    const ByteNode& n = nodes[idx];
    if (n.is_leaf) {
      return (int32_t)n.base_weight +
             (int32_t)n.scale_weight * (int32_t)r.counts[n.scale_reg];
    }
    if (evaluate_byte_predicate(n.predicate, r)) {
      return evaluate_side(r, n.yes_idx);
    }
    return evaluate_side(r, n.no_idx);
  }

  int count_nodes() const { return (int)nodes.size(); }

  void compact() {
    if (nodes.empty()) return;
    std::vector<ByteNode> active;
    active.reserve(nodes.size());

    auto copy_reach = [&](auto& self, int old_idx, uint32_t known_t,
                          uint32_t known_f, int depth) -> int {
      if (old_idx < 0 || old_idx >= (int)nodes.size() || depth > 16) return -1;
      const ByteNode& src = nodes[old_idx];
      if (!src.is_leaf) {
        uint32_t bit = 1U << (int)src.predicate;
        if (known_t & bit)
          return self(self, src.yes_idx, known_t, known_f, depth + 1);
        if (known_f & bit)
          return self(self, src.no_idx, known_t, known_f, depth + 1);

        int n_idx = (int)active.size();
        active.push_back(src);
        int y = self(self, src.yes_idx, known_t | bit, known_f, depth + 1);
        int n = self(self, src.no_idx, known_t, known_f | bit, depth + 1);

        if (y >= 0 && n >= 0 && active[y].is_leaf && active[n].is_leaf &&
            active[y].base_weight == active[n].base_weight &&
            active[y].scale_reg == active[n].scale_reg &&
            active[y].scale_weight == active[n].scale_weight && n_idx > 0) {
          active[n_idx] = active[y];
          return n_idx;
        }

        active[n_idx].yes_idx = y;
        active[n_idx].no_idx = n;
        return n_idx;
      }
      int n_idx = (int)active.size();
      active.push_back(src);
      return n_idx;
    };

    copy_reach(copy_reach, 0, 0U, 0U, 0);
    nodes = std::move(active);
  }

  std::string to_json(int idx = 0) const {
    if (nodes.empty() || idx < 0 || idx >= (int)nodes.size()) return "null";
    const ByteNode& n = nodes[idx];
    if (n.is_leaf) {
      return "{\"is_leaf\":true,\"weight\":" + std::to_string(n.base_weight) +
             ",\"scale_reg\":" + std::to_string(n.scale_reg) +
             ",\"scale_name\":\"" + std::string(SCALE_REG_NAMES[n.scale_reg]) +
             "\",\"scale_weight\":" + std::to_string(n.scale_weight) + "}";
    }
    return "{\"is_leaf\":false,\"predicate\":\"" +
           std::string(BYTE_PRED_NAMES[n.predicate]) +
           "\",\"predicate_id\":" + std::to_string(n.predicate) +
           ",\"yes\":" + to_json(n.yes_idx) + ",\"no\":" + to_json(n.no_idx) +
           "}";
  }
};

static constexpr int32_t CENTRALITY_TABLE[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,
    0,   0,   -10, -10, 0,   10,  15,  15,  10,  0,   -10, -10, 5,
    15,  22,  22,  15,  5,   -10, -10, 5,   15,  22,  22,  15,  5,
    -10, -10, 0,   10,  15,  15,  10,  0,   -10, -10, 0,   0,   0,
    0,   0,   0,   -10, -20, -10, -10, -10, -10, -10, -10, -20};

class ChessMarkovEvaluator {
 public:
  int32_t pawn_pmi[5][5];
  int32_t king_shield_pmi[5][5];

  ChessMarkovEvaluator() {
    const int V = 25;
    const double alpha = 1.0;
    double p_counts[5][5] = {{1.0, 4.0, 1.0, 4.0, 1.0},
                             {3.0, 35.0, 1.0, 35.0, 3.0},
                             {6.0, 28.0, 1.0, 28.0, 6.0},
                             {3.0, 35.0, 1.0, 35.0, 3.0},
                             {1.0, 4.0, 1.0, 4.0, 1.0}};
    double k_counts[5][5] = {{1.0, 2.0, 2.0, 2.0, 1.0},
                             {4.0, 40.0, 45.0, 40.0, 4.0},
                             {2.0, 15.0, 1.0, 15.0, 2.0},
                             {1.0, 3.0, 3.0, 3.0, 1.0},
                             {1.0, 1.0, 1.0, 1.0, 1.0}};

    double p_sum = 0, k_sum = 0;
    for (int r = 0; r < 5; ++r)
      for (int c = 0; c < 5; ++c) {
        p_sum += p_counts[r][c];
        k_sum += k_counts[r][c];
      }

    for (int r = 0; r < 5; ++r) {
      for (int c = 0; c < 5; ++c) {
        double pp = (p_counts[r][c] + alpha) / (p_sum + alpha * V);
        double pk = (k_counts[r][c] + alpha) / (k_sum + alpha * V);
        pawn_pmi[r][c] = (int32_t)std::round(std::log2(pp * V) * 5.0);
        king_shield_pmi[r][c] = (int32_t)std::round(std::log2(pk * V) * 9.0);
      }
    }
  }

  int32_t evaluate_side(const ChessBoard& b, bool is_white) const {
    int off = is_white ? 0 : 6;
    int them = is_white ? 6 : 0;
    uint64_t pawns = b.bb[off + 0];
    uint64_t enemy_pawns = b.bb[them + 0];
    uint64_t minors = b.bb[off + 1] | b.bb[off + 2];
    uint64_t king = b.bb[off + 5];
    int32_t score = 0;

    while (minors) {
      int sq = __builtin_ctzll(minors);
      minors &= minors - 1;
      score += CENTRALITY_TABLE[sq];
    }

    const uint64_t center4 = 0x0000001818000000ULL;
    score += __builtin_popcountll(pawns & center4) * 18;

    static constexpr int32_t PASSED_RANK_BONUS[8] = {0,  0,  10,  24,
                                                     48, 85, 145, 0};

    uint64_t p_iter = pawns;
    while (p_iter) {
      int sq = __builtin_ctzll(p_iter);
      p_iter &= p_iter - 1;
      int r = sq / 8, c = sq % 8;
      int adv = is_white ? r : (7 - r);
      if (c >= 2 && c <= 5)
        score += adv * 5;
      else
        score += adv * 2;

      uint64_t f_span = ATTACKS.file_masks[c] | ATTACKS.adjacent_files[c];
      uint64_t front_mask =
          is_white ? (~0ULL << ((r + 1) * 8)) : ((1ULL << (r * 8)) - 1ULL);
      if (!(enemy_pawns & f_span & front_mask)) {
        score += PASSED_RANK_BONUS[adv];
      }

      for (int dr = -2; dr <= 2; ++dr) {
        for (int dc = -2; dc <= 2; ++dc) {
          if (dr == 0 && dc == 0) continue;
          int nr = r + (is_white ? dr : -dr);
          int nc = c + dc;
          if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
            if (pawns & (1ULL << (nr * 8 + nc))) {
              score += pawn_pmi[2 - dr][dc + 2];
            }
          }
        }
      }
    }

    if (king) {
      int ksq = __builtin_ctzll(king);
      int kr = ksq / 8, kc = ksq % 8;
      int back_r = is_white ? 0 : 7;
      bool has_castle =
          is_white ? ((b.castling & 3) != 0) : ((b.castling & 12) != 0);
      int phi = b.game_phase();

      if (phi >= 12) {
        if (kr == back_r && (kc >= 5 || kc <= 2)) {
          score += 45;
          for (int dr = -2; dr <= 2; ++dr) {
            for (int dc = -2; dc <= 2; ++dc) {
              if (dr == 0 && dc == 0) continue;
              int nr = kr + (is_white ? dr : -dr);
              int nc = kc + dc;
              if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                if (pawns & (1ULL << (nr * 8 + nc))) {
                  score += king_shield_pmi[2 - dr][dc + 2];
                }
              }
            }
          }
        } else if (!has_castle) {
          score -= 50;
        } else {
          int f_sq = is_white ? 13 : 53;
          int g_sq = is_white ? 14 : 54;
          if (!(pawns & (1ULL << f_sq))) score -= 35;
          if (!(pawns & (1ULL << g_sq))) score -= 25;
        }
      } else {
        score += CENTRALITY_TABLE[ksq];
      }
    }
    return score;
  }

  int32_t evaluate(const ChessBoard& b) const {
    return evaluate_side(b, true) - evaluate_side(b, false);
  }
};

static const ChessMarkovEvaluator CHESS_MARKOV;