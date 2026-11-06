#pragma once
#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

enum Piece {
  W_PAWN = 0,
  W_KNIGHT = 1,
  W_BISHOP = 2,
  W_ROOK = 3,
  W_QUEEN = 4,
  W_KING = 5,
  B_PAWN = 6,
  B_KNIGHT = 7,
  B_BISHOP = 8,
  B_ROOK = 9,
  B_QUEEN = 10,
  B_KING = 11,
  NO_PIECE = 12
};

struct ZobristTable {
  uint64_t piece_sq[12][64];
  uint64_t side_hash;
  uint64_t castle_hash[16];
  uint64_t ep_hash[65];

  ZobristTable() {
    std::mt19937_64 rng(0xCAFEBABE2026ULL);
    for (int p = 0; p < 12; ++p)
      for (int s = 0; s < 64; ++s) piece_sq[p][s] = rng();
    side_hash = rng();
    for (int c = 0; c < 16; ++c) castle_hash[c] = rng();
    for (int e = 0; e < 65; ++e) ep_hash[e] = rng();
  }
};

static const ZobristTable ZOBRIST;

inline uint8_t project_to_file_byte(uint64_t bb) {
  return static_cast<uint8_t>((bb * 0x0101010101010101ULL) >> 56);
}

inline uint8_t extract_rank_byte(uint64_t bb, int rank) {
  return static_cast<uint8_t>((bb >> (rank * 8)) & 0xFFULL);
}

struct ChessBoard {
  uint64_t bb[12] = {0};
  bool white_to_move = true;
  uint8_t castling = 0xF;
  int8_t ep_sq = -1;

  inline uint64_t white_occ() const {
    return bb[0] | bb[1] | bb[2] | bb[3] | bb[4] | bb[5];
  }

  inline uint64_t black_occ() const {
    return bb[6] | bb[7] | bb[8] | bb[9] | bb[10] | bb[11];
  }

  inline uint64_t all_occ() const { return white_occ() | black_occ(); }

  int piece_at(int sq) const {
    uint64_t mask = 1ULL << sq;
    for (int p = 0; p < 12; ++p) {
      if (bb[p] & mask) return p;
    }
    return NO_PIECE;
  }

  int game_phase() const {
    int phase = 0;
    phase += __builtin_popcountll(bb[W_KNIGHT] | bb[B_KNIGHT]) * 1;
    phase += __builtin_popcountll(bb[W_BISHOP] | bb[B_BISHOP]) * 1;
    phase += __builtin_popcountll(bb[W_ROOK] | bb[B_ROOK]) * 2;
    phase += __builtin_popcountll(bb[W_QUEEN] | bb[B_QUEEN]) * 4;
    return std::min(24, phase);
  }

  uint64_t compute_hash() const {
    uint64_t h = white_to_move ? ZOBRIST.side_hash : 0ULL;
    h ^= ZOBRIST.castle_hash[castling & 0xF];
    h ^= ZOBRIST.ep_hash[(ep_sq >= 0 && ep_sq < 64) ? ep_sq : 64];
    for (int p = 0; p < 12; ++p) {
      uint64_t b = bb[p];
      while (b) {
        int sq = __builtin_ctzll(b);
        b &= b - 1;
        h ^= ZOBRIST.piece_sq[p][sq];
      }
    }
    return h;
  }
};

struct Move {
  uint8_t from = 0;
  uint8_t to = 0;
  uint8_t moved_piece = NO_PIECE;
  uint8_t captured_piece = NO_PIECE;
  uint8_t promoted_piece = NO_PIECE;
  bool is_castling = false;
  bool is_ep = false;
};

struct AttackTables {
  uint64_t knight_attacks[64];
  uint64_t king_attacks[64];
  uint64_t pawn_attacks[2][64];
  uint64_t file_masks[8];
  uint64_t rank_masks[8];
  uint64_t adjacent_files[8];

  AttackTables() {
    for (int i = 0; i < 8; ++i) {
      file_masks[i] = 0x0101010101010101ULL << i;
      rank_masks[i] = 0xFFULL << (i * 8);
    }
    for (int i = 0; i < 8; ++i) {
      adjacent_files[i] = 0;
      if (i > 0) adjacent_files[i] |= file_masks[i - 1];
      if (i < 7) adjacent_files[i] |= file_masks[i + 1];
    }
    const int kr[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
    const int kc[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
    const int gr[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    const int gc[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

    for (int sq = 0; sq < 64; ++sq) {
      int r = sq / 8, c = sq % 8;
      uint64_t na = 0, ka = 0, wpa = 0, bpa = 0;
      for (int i = 0; i < 8; ++i) {
        int nr = r + kr[i], nc = c + kc[i];
        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8)
          na |= (1ULL << (nr * 8 + nc));
        int mr = r + gr[i], mc = c + gc[i];
        if (mr >= 0 && mr < 8 && mc >= 0 && mc < 8)
          ka |= (1ULL << (mr * 8 + mc));
      }
      if (r < 7) {
        if (c > 0) wpa |= (1ULL << ((r + 1) * 8 + (c - 1)));
        if (c < 7) wpa |= (1ULL << ((r + 1) * 8 + (c + 1)));
      }
      if (r > 0) {
        if (c > 0) bpa |= (1ULL << ((r - 1) * 8 + (c - 1)));
        if (c < 7) bpa |= (1ULL << ((r - 1) * 8 + (c + 1)));
      }
      knight_attacks[sq] = na;
      king_attacks[sq] = ka;
      pawn_attacks[0][sq] = wpa;
      pawn_attacks[1][sq] = bpa;
    }
  }
};

static const AttackTables ATTACKS;

inline uint64_t slide_attacks(int sq, uint64_t occ, bool ortho) {
  uint64_t attacks = 0;
  int r = sq / 8, c = sq % 8;
  const int dr[8] = {1, -1, 0, 0, 1, 1, -1, -1};
  const int dc[8] = {0, 0, 1, -1, 1, -1, 1, -1};
  int start = ortho ? 0 : 4;
  int end = ortho ? 4 : 8;

  for (int d = start; d < end; ++d) {
    int nr = r + dr[d], nc = c + dc[d];
    while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
      uint64_t bit = 1ULL << (nr * 8 + nc);
      attacks |= bit;
      if (occ & bit) break;
      nr += dr[d];
      nc += dc[d];
    }
  }
  return attacks;
}

inline bool is_square_attacked(const ChessBoard& b, int sq, bool by_white) {
  int offset = by_white ? 0 : 6;
  uint64_t occ = b.all_occ();
  if (ATTACKS.pawn_attacks[by_white ? 1 : 0][sq] & b.bb[offset + 0])
    return true;
  if (ATTACKS.knight_attacks[sq] & b.bb[offset + 1]) return true;
  if (ATTACKS.king_attacks[sq] & b.bb[offset + 5]) return true;
  uint64_t diag = slide_attacks(sq, occ, false);
  if (diag & (b.bb[offset + 2] | b.bb[offset + 4])) return true;
  uint64_t ortho = slide_attacks(sq, occ, true);
  if (ortho & (b.bb[offset + 3] | b.bb[offset + 4])) return true;
  return false;
}

inline bool is_in_check(const ChessBoard& b, bool white_king) {
  uint64_t k_bb = b.bb[white_king ? W_KING : B_KING];
  if (!k_bb) return true;
  int ksq = __builtin_ctzll(k_bb);
  return is_square_attacked(b, ksq, !white_king);
}

inline ChessBoard apply_move(const ChessBoard& b, const Move& m) {
  ChessBoard next = b;
  next.ep_sq = -1;
  uint64_t from_mask = 1ULL << m.from;
  uint64_t to_mask = 1ULL << m.to;

  next.bb[m.moved_piece] &= ~from_mask;
  if (m.is_ep) {
    int cap_sq = b.white_to_move ? (m.to - 8) : (m.to + 8);
    next.bb[m.captured_piece] &= ~(1ULL << cap_sq);
  } else if (m.captured_piece != NO_PIECE) {
    next.bb[m.captured_piece] &= ~to_mask;
  }

  if (m.promoted_piece != NO_PIECE) {
    next.bb[m.promoted_piece] |= to_mask;
  } else {
    next.bb[m.moved_piece] |= to_mask;
  }

  if (m.moved_piece == W_PAWN && (m.to - m.from) == 16)
    next.ep_sq = m.from + 8;
  else if (m.moved_piece == B_PAWN && (m.from - m.to) == 16)
    next.ep_sq = m.from - 8;

  if (m.is_castling) {
    if (m.to == 6) {
      next.bb[W_ROOK] &= ~(1ULL << 7);
      next.bb[W_ROOK] |= (1ULL << 5);
    } else if (m.to == 2) {
      next.bb[W_ROOK] &= ~(1ULL << 0);
      next.bb[W_ROOK] |= (1ULL << 3);
    } else if (m.to == 62) {
      next.bb[B_ROOK] &= ~(1ULL << 63);
      next.bb[B_ROOK] |= (1ULL << 61);
    } else if (m.to == 58) {
      next.bb[B_ROOK] &= ~(1ULL << 56);
      next.bb[B_ROOK] |= (1ULL << 59);
    }
  }

  if (m.moved_piece == W_KING) next.castling &= ~3;
  if (m.moved_piece == B_KING) next.castling &= ~12;
  if (m.from == 0 || m.to == 0) next.castling &= ~2;
  if (m.from == 7 || m.to == 7) next.castling &= ~1;
  if (m.from == 56 || m.to == 56) next.castling &= ~8;
  if (m.from == 63 || m.to == 63) next.castling &= ~4;

  next.white_to_move = !b.white_to_move;
  return next;
}

inline void generate_legal_moves(const ChessBoard& b,
                                 std::vector<Move>& legal_moves,
                                 bool captures_only = false) {
  legal_moves.clear();
  std::vector<Move> pseudo;
  pseudo.reserve(64);

  bool w = b.white_to_move;
  uint64_t us = w ? b.white_occ() : b.black_occ();
  uint64_t them = w ? b.black_occ() : b.white_occ();
  uint64_t all = us | them;
  int offset = w ? 0 : 6;
  int enemy_offset = w ? 6 : 0;

  uint64_t pawns = b.bb[offset + 0];
  while (pawns) {
    int from = __builtin_ctzll(pawns);
    pawns &= pawns - 1;
    int r = from / 8, c = from % 8;
    int forward = w ? (from + 8) : (from - 8);

    if (!captures_only && forward >= 0 && forward < 64 &&
        !(all & (1ULL << forward))) {
      uint8_t promo =
          ((w && r == 6) || (!w && r == 1)) ? (offset + 4) : NO_PIECE;
      pseudo.push_back({(uint8_t)from, (uint8_t)forward, (uint8_t)(offset + 0),
                        NO_PIECE, promo, false, false});
      int start_rank = w ? 1 : 6;
      int double_step = w ? (from + 16) : (from - 16);
      if (r == start_rank && !(all & (1ULL << double_step))) {
        pseudo.push_back({(uint8_t)from, (uint8_t)double_step,
                          (uint8_t)(offset + 0), NO_PIECE, NO_PIECE, false,
                          false});
      }
    }

    for (int dc : {-1, 1}) {
      if (c + dc < 0 || c + dc >= 8) continue;
      int cap_sq = forward + dc;
      if (cap_sq >= 0 && cap_sq < 64) {
        if (them & (1ULL << cap_sq)) {
          uint8_t cap_p = (uint8_t)b.piece_at(cap_sq);
          uint8_t promo =
              ((w && r == 6) || (!w && r == 1)) ? (offset + 4) : NO_PIECE;
          pseudo.push_back({(uint8_t)from, (uint8_t)cap_sq,
                            (uint8_t)(offset + 0), cap_p, promo, false, false});
        } else if (cap_sq == b.ep_sq) {
          pseudo.push_back({(uint8_t)from, (uint8_t)cap_sq,
                            (uint8_t)(offset + 0), (uint8_t)(enemy_offset + 0),
                            NO_PIECE, false, true});
        }
      }
    }
  }

  for (int pt = 1; pt <= 5; ++pt) {
    uint64_t pieces = b.bb[offset + pt];
    while (pieces) {
      int from = __builtin_ctzll(pieces);
      pieces &= pieces - 1;
      uint64_t atk = 0;
      if (pt == 1)
        atk = ATTACKS.knight_attacks[from];
      else if (pt == 2)
        atk = slide_attacks(from, all, false);
      else if (pt == 3)
        atk = slide_attacks(from, all, true);
      else if (pt == 4)
        atk = slide_attacks(from, all, false) | slide_attacks(from, all, true);
      else if (pt == 5)
        atk = ATTACKS.king_attacks[from];

      atk &= ~us;
      if (captures_only) atk &= them;

      while (atk) {
        int to = __builtin_ctzll(atk);
        atk &= atk - 1;
        uint8_t cap_p =
            (them & (1ULL << to)) ? (uint8_t)b.piece_at(to) : NO_PIECE;
        pseudo.push_back({(uint8_t)from, (uint8_t)to, (uint8_t)(offset + pt),
                          cap_p, NO_PIECE, false, false});
      }
    }
  }

  if (!captures_only) {
    if (w) {
      if ((b.castling & 1) && !(all & 0x60ULL) &&
          !is_square_attacked(b, 4, false) &&
          !is_square_attacked(b, 5, false) &&
          !is_square_attacked(b, 6, false)) {
        pseudo.push_back({4, 6, W_KING, NO_PIECE, NO_PIECE, true, false});
      }
      if ((b.castling & 2) && !(all & 0x0EULL) &&
          !is_square_attacked(b, 4, false) &&
          !is_square_attacked(b, 3, false) &&
          !is_square_attacked(b, 2, false)) {
        pseudo.push_back({4, 2, W_KING, NO_PIECE, NO_PIECE, true, false});
      }
    } else {
      if ((b.castling & 4) && !(all & 0x6000000000000000ULL) &&
          !is_square_attacked(b, 60, true) &&
          !is_square_attacked(b, 61, true) &&
          !is_square_attacked(b, 62, true)) {
        pseudo.push_back({60, 62, B_KING, NO_PIECE, NO_PIECE, true, false});
      }
      if ((b.castling & 8) && !(all & 0x0E00000000000000ULL) &&
          !is_square_attacked(b, 60, true) &&
          !is_square_attacked(b, 59, true) &&
          !is_square_attacked(b, 58, true)) {
        pseudo.push_back({60, 58, B_KING, NO_PIECE, NO_PIECE, true, false});
      }
    }
  }

  for (const auto& m : pseudo) {
    if (m.captured_piece == W_KING || m.captured_piece == B_KING) continue;
    ChessBoard next = apply_move(b, m);
    if (!is_in_check(next, w)) {
      legal_moves.push_back(m);
    }
  }
}