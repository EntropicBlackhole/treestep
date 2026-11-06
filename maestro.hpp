#pragma once
#include <fstream>
#include <sstream>

#include "bytetree.hpp"

inline int32_t material_score_side(const ChessBoard& b, int offset) {
  return __builtin_popcountll(b.bb[offset + 0]) * 100 +
         __builtin_popcountll(b.bb[offset + 1]) * 320 +
         __builtin_popcountll(b.bb[offset + 2]) * 330 +
         __builtin_popcountll(b.bb[offset + 3]) * 500 +
         __builtin_popcountll(b.bb[offset + 4]) * 900;
}

inline int build_random_byte_subtree(SymbolicByteTree& tree, int depth,
                                     int level, std::mt19937& rng) {
  std::uniform_int_distribution<int> pred_dist(0, BYTE_PRED_COUNT - 1);
  std::uniform_int_distribution<int> sreg_dist(0, SCALE_REG_COUNT - 1);
  std::uniform_int_distribution<int16_t> base_dist(-25, 35);
  std::uniform_int_distribution<int16_t> scale_dist(2, 14);
  std::uniform_real_distribution<float> prob(0.0f, 1.0f);

  int curr_idx = (int)tree.nodes.size();
  tree.nodes.push_back(ByteNode{});

  if (level > 0 && (depth <= 0 || prob(rng) < 0.25f)) {
    tree.nodes[curr_idx].is_leaf = true;
    tree.nodes[curr_idx].base_weight = base_dist(rng);
    tree.nodes[curr_idx].scale_reg = (ScaleRegType)sreg_dist(rng);
    tree.nodes[curr_idx].scale_weight = scale_dist(rng);
    return curr_idx;
  }

  tree.nodes[curr_idx].is_leaf = false;
  tree.nodes[curr_idx].predicate = (BytePredicate)pred_dist(rng);
  int yes_c = build_random_byte_subtree(tree, depth - 1, level + 1, rng);
  int no_c = build_random_byte_subtree(tree, depth - 1, level + 1, rng);
  tree.nodes[curr_idx].yes_idx = yes_c;
  tree.nodes[curr_idx].no_idx = no_c;
  return curr_idx;
}

inline SymbolicByteTree create_random_byte_tree(int max_depth,
                                                std::mt19937& rng) {
  SymbolicByteTree t;
  t.nodes.reserve(15);
  build_random_byte_subtree(t, std::max(1, max_depth), 0, rng);
  t.compact();
  return t;
}

inline SymbolicByteTree create_seeded_byte_specialist(int role, bool endgame) {
  SymbolicByteTree t;
  if (!endgame) {
    if (role == 0) {
      t.nodes.push_back(
          {false, BYTE_CASTLE_READY_OR_DONE, 0, REG_KING_SHIELD, 0, 1, 2});
      t.nodes.push_back(
          {false, BYTE_KING_SHIELD_2PLUS, 0, REG_KING_SHIELD, 0, 3, 4});
      t.nodes.push_back(
          {true, BYTE_CASTLE_READY_OR_DONE, -20, REG_DEV_MINORS, 8, -1, -1});
      t.nodes.push_back(
          {true, BYTE_CASTLE_READY_OR_DONE, 30, REG_KING_SHIELD, 12, -1, -1});
      t.nodes.push_back(
          {true, BYTE_CASTLE_READY_OR_DONE, 10, REG_KING_SHIELD, 8, -1, -1});
    } else if (role == 1) {
      t.nodes.push_back(
          {false, BYTE_CENTER_FILES_OCC, 0, REG_CENTER_OCC, 0, 1, 2});
      t.nodes.push_back(
          {false, BYTE_BOTH_D_E_FILES, 0, REG_CENTER_OCC, 0, 3, 4});
      t.nodes.push_back(
          {true, BYTE_CENTER_FILES_OCC, -15, REG_CENTER_OCC, 10, -1, -1});
      t.nodes.push_back(
          {true, BYTE_CENTER_FILES_OCC, 25, REG_CENTER_OCC, 14, -1, -1});
      t.nodes.push_back(
          {true, BYTE_CENTER_FILES_OCC, 12, REG_PHALANX_FILES, 10, -1, -1});
    } else {
      t.nodes.push_back(
          {false, BYTE_BISHOP_MOB_7PLUS, 0, REG_BISHOP_MOBILITY, 0, 1, 2});
      t.nodes.push_back(
          {false, BYTE_DEV_MINORS_2PLUS, 0, REG_DEV_MINORS, 0, 3, 4});
      t.nodes.push_back(
          {true, BYTE_BISHOP_MOB_7PLUS, -15, REG_BISHOP_MOBILITY, 4, -1, -1});
      t.nodes.push_back(
          {true, BYTE_BISHOP_MOB_7PLUS, 25, REG_DEV_MINORS, 14, -1, -1});
      t.nodes.push_back(
          {true, BYTE_BISHOP_MOB_7PLUS, 10, REG_KNIGHT_MOBILITY, 5, -1, -1});
    }
  } else {
    if (role == 0) {
      t.nodes.push_back(
          {false, BYTE_ENDGAME_KING_CENTRAL, 0, REG_KING_CENTRALITY, 0, 1, 2});
      t.nodes.push_back({true, BYTE_ENDGAME_KING_CENTRAL, 25,
                         REG_KING_CENTRALITY, 14, -1, -1});
      t.nodes.push_back({true, BYTE_ENDGAME_KING_CENTRAL, -20,
                         REG_KING_CENTRALITY, 8, -1, -1});
    } else if (role == 1) {
      t.nodes.push_back(
          {false, BYTE_PASSED_FILE_EXISTS, 0, REG_PASSED_PAWNS, 0, 1, 2});
      t.nodes.push_back(
          {true, BYTE_PASSED_FILE_EXISTS, 35, REG_PASSED_PAWNS, 25, -1, -1});
      t.nodes.push_back(
          {true, BYTE_PASSED_FILE_EXISTS, -10, REG_PHALANX_FILES, 8, -1, -1});
    } else {
      t.nodes.push_back(
          {false, BYTE_SEVENTH_RANK_ACTIVE, 0, REG_SEVENTH_INVASION, 0, 1, 2});
      t.nodes.push_back({true, BYTE_SEVENTH_RANK_ACTIVE, 30,
                         REG_SEVENTH_INVASION, 20, -1, -1});
      t.nodes.push_back(
          {true, BYTE_SEVENTH_RANK_ACTIVE, 10, REG_OPEN_ROOKS, 12, -1, -1});
    }
  }
  return t;
}

inline int copy_byte_subtree(const SymbolicByteTree& src, int src_idx,
                             SymbolicByteTree& dst) {
  int dst_idx = (int)dst.nodes.size();
  ByteNode node = src.nodes[src_idx];
  dst.nodes.push_back(node);
  if (!node.is_leaf) {
    int y = copy_byte_subtree(src, node.yes_idx, dst);
    int n = copy_byte_subtree(src, node.no_idx, dst);
    dst.nodes[dst_idx].yes_idx = y;
    dst.nodes[dst_idx].no_idx = n;
  }
  return dst_idx;
}

inline SymbolicByteTree crossover_byte_tree(const SymbolicByteTree& p1,
                                            const SymbolicByteTree& p2,
                                            std::mt19937& rng) {
  if (p1.nodes.size() <= 1 || p2.nodes.size() <= 1) return p1;
  std::uniform_int_distribution<int> d1(1, (int)p1.nodes.size() - 1);
  std::uniform_int_distribution<int> d2(1, (int)p2.nodes.size() - 1);
  int replace_target = d1(rng);
  int donor_root = d2(rng);

  SymbolicByteTree child;
  child.nodes.reserve(24);

  auto rebuild = [&](auto& self, int s_idx, int depth) -> int {
    if (s_idx == replace_target || depth >= 4) {
      return copy_byte_subtree(p2, donor_root, child);
    }
    int c_idx = (int)child.nodes.size();
    ByteNode n = p1.nodes[s_idx];
    child.nodes.push_back(n);
    if (!n.is_leaf) {
      int y = self(self, n.yes_idx, depth + 1);
      int no = self(self, n.no_idx, depth + 1);
      child.nodes[c_idx].yes_idx = y;
      child.nodes[c_idx].no_idx = no;
    }
    return c_idx;
  };

  rebuild(rebuild, 0, 0);
  child.compact();
  return child;
}

inline void mutate_byte_tree(SymbolicByteTree& t, std::mt19937& rng) {
  if (t.nodes.empty()) return;
  std::uniform_int_distribution<int> idx_dist(0, (int)t.nodes.size() - 1);
  std::uniform_int_distribution<int> pred_dist(0, BYTE_PRED_COUNT - 1);
  std::uniform_int_distribution<int> sreg_dist(0, SCALE_REG_COUNT - 1);
  std::uniform_int_distribution<int16_t> delta_base(-12, 12);
  std::uniform_int_distribution<int16_t> delta_scale(-4, 4);
  std::uniform_int_distribution<int16_t> base_dist(-25, 35);
  std::uniform_int_distribution<int16_t> scale_dist(2, 14);
  std::uniform_real_distribution<float> prob(0.0f, 1.0f);

  int idx = idx_dist(rng);
  if (t.nodes[idx].is_leaf) {
    if (t.nodes.size() < 15 && prob(rng) < 0.22f) {
      int yes_c = (int)t.nodes.size();
      t.nodes.push_back({true, BYTE_CENTER_FILES_OCC, base_dist(rng),
                         (ScaleRegType)sreg_dist(rng), scale_dist(rng), -1,
                         -1});
      int no_c = (int)t.nodes.size();
      t.nodes.push_back({true, BYTE_CENTER_FILES_OCC, base_dist(rng),
                         (ScaleRegType)sreg_dist(rng), scale_dist(rng), -1,
                         -1});
      t.nodes[idx].is_leaf = false;
      t.nodes[idx].predicate = (BytePredicate)pred_dist(rng);
      t.nodes[idx].yes_idx = yes_c;
      t.nodes[idx].no_idx = no_c;
    } else if (prob(rng) < 0.30f) {
      t.nodes[idx].scale_reg = (ScaleRegType)sreg_dist(rng);
    } else {
      t.nodes[idx].base_weight = std::clamp<int16_t>(
          t.nodes[idx].base_weight + delta_base(rng), -60, 60);
      t.nodes[idx].scale_weight = std::clamp<int16_t>(
          t.nodes[idx].scale_weight + delta_scale(rng), -10, 25);
    }
  } else {
    t.nodes[idx].predicate = (BytePredicate)pred_dist(rng);
  }
  t.compact();
}

inline int parse_json_int_field(const std::string& s, const std::string& key,
                                size_t start_pos, size_t end_pos,
                                int default_val = 0) {
  size_t kpos = s.find("\"" + key + "\":", start_pos);
  if (kpos == std::string::npos || kpos >= end_pos) return default_val;
  kpos += key.length() + 3;
  while (kpos < end_pos && (s[kpos] == ' ' || s[kpos] == '\t')) kpos++;
  return std::stoi(s.substr(kpos, 12));
}

inline int parse_json_tree_recursive(const std::string& s, size_t& pos,
                                     SymbolicByteTree& tree) {
  size_t open_brace = s.find('{', pos);
  if (open_brace == std::string::npos) return -1;

  int curr_idx = (int)tree.nodes.size();
  tree.nodes.push_back(ByteNode{});

  size_t leaf_pos = s.find("\"is_leaf\":", open_brace);
  bool is_leaf = (s.substr(leaf_pos + 10, 5).find("true") != std::string::npos);

  if (is_leaf) {
    size_t close_brace = s.find('}', open_brace);
    tree.nodes[curr_idx].is_leaf = true;
    tree.nodes[curr_idx].base_weight =
        (int16_t)parse_json_int_field(s, "weight", open_brace, close_brace, 0);
    tree.nodes[curr_idx].scale_reg = (ScaleRegType)std::clamp(
        parse_json_int_field(s, "scale_reg", open_brace, close_brace, 0), 0,
        SCALE_REG_COUNT - 1);
    tree.nodes[curr_idx].scale_weight = (int16_t)parse_json_int_field(
        s, "scale_weight", open_brace, close_brace, 0);
    pos = close_brace + 1;
    return curr_idx;
  }

  size_t yes_key = s.find("\"yes\":", open_brace);
  tree.nodes[curr_idx].is_leaf = false;
  int pid = std::clamp(
      parse_json_int_field(s, "predicate_id", open_brace, yes_key, 0), 0,
      BYTE_PRED_COUNT - 1);
  tree.nodes[curr_idx].predicate = (BytePredicate)pid;

  pos = yes_key + 6;
  int y_idx = parse_json_tree_recursive(s, pos, tree);

  size_t no_key = s.find("\"no\":", pos);
  pos = no_key + 5;
  int n_idx = parse_json_tree_recursive(s, pos, tree);

  size_t close_brace = s.find('}', pos);
  pos = (close_brace != std::string::npos) ? (close_brace + 1) : pos;

  tree.nodes[curr_idx].yes_idx = y_idx;
  tree.nodes[curr_idx].no_idx = n_idx;
  return curr_idx;
}

struct ChessMaestro {
  SymbolicByteTree lado_A[3];
  SymbolicByteTree lado_B[3];
  int64_t fitness = 0;

  int total_nodes() const {
    int s = 0;
    for (int i = 0; i < 3; ++i)
      s += lado_A[i].count_nodes() + lado_B[i].count_nodes();
    return s;
  }

  int32_t evaluate_quiet_board(const ChessBoard& b) const {
    int32_t mat_w = material_score_side(b, 0);
    int32_t mat_b = material_score_side(b, 6);
    int32_t mat_diff = mat_w - mat_b;
    int32_t markov_diff = CHESS_MARKOV.evaluate(b);

    SideByteRegisters w_regs = extract_side_bytes(b, true);
    SideByteRegisters b_regs = extract_side_bytes(b, false);

    int32_t midgame_trees = 0;
    int32_t endgame_trees = 0;
    for (int i = 0; i < 3; ++i) {
      midgame_trees += (lado_A[i].evaluate_side(w_regs, 0) -
                        lado_A[i].evaluate_side(b_regs, 0));
      endgame_trees += (lado_B[i].evaluate_side(w_regs, 0) -
                        lado_B[i].evaluate_side(b_regs, 0));
    }

    int phi = b.game_phase();
    int32_t tapered_trees = std::clamp(
        (phi * midgame_trees + (24 - phi) * endgame_trees) / 24, -160, 160);

    int32_t mop_up = 0;
    if (std::abs(mat_diff) >= 200 && b.bb[W_KING] && b.bb[B_KING]) {
      int wk = __builtin_ctzll(b.bb[W_KING]);
      int bk = __builtin_ctzll(b.bb[B_KING]);
      int k_dist =
          std::abs((wk / 8) - (bk / 8)) + std::abs((wk % 8) - (bk % 8));
      int trade_bonus = (24 - phi) * 5;
      if (mat_diff > 0) {
        int bk_edge = std::max(std::abs((bk / 8) - 3), std::abs((bk % 8) - 3));
        mop_up = trade_bonus + bk_edge * 12 + (14 - k_dist) * 6;
      } else {
        int wk_edge = std::max(std::abs((wk / 8) - 3), std::abs((wk % 8) - 3));
        mop_up = -(trade_bonus + wk_edge * 12 + (14 - k_dist) * 6);
      }
    }

    int32_t white_perspective = mat_diff + markov_diff + tapered_trees + mop_up;
    return b.white_to_move ? white_perspective : -white_perspective;
  }

  int32_t quiescence(const ChessBoard& b, int32_t alpha, int32_t beta,
                     int q_depth = 0) const {
    bool in_chk = is_in_check(b, b.white_to_move);
    if (!in_chk) {
      int32_t stand_pat = evaluate_quiet_board(b);
      if (stand_pat >= beta || q_depth >= 6) return stand_pat;
      if (alpha < stand_pat) alpha = stand_pat;
    } else if (q_depth >= 6) {
      return evaluate_quiet_board(b);
    }

    std::vector<Move> moves;
    generate_legal_moves(b, moves, !in_chk);
    if (in_chk && moves.empty()) return -25000 + q_depth * 50;

    std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b_mv) {
      return (a.captured_piece % 6) > (b_mv.captured_piece % 6);
    });

    for (const auto& m : moves) {
      ChessBoard next = apply_move(b, m);
      int32_t score = -quiescence(next, -beta, -alpha, q_depth + 1);
      if (score >= beta) return beta;
      if (score > alpha) alpha = score;
    }
    return alpha;
  }

  int32_t negamax(const ChessBoard& b, int depth, int32_t alpha, int32_t beta,
                  const std::vector<uint64_t>& history) const {
    if (depth <= 0) return quiescence(b, alpha, beta, 0);

    std::vector<Move> moves;
    generate_legal_moves(b, moves, false);

    if (moves.empty()) {
      if (is_in_check(b, b.white_to_move)) return -25000 - depth * 50;
      return 0;
    }

    std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b_mv) {
      int sa = (a.captured_piece != NO_PIECE)
                   ? (100 + (a.captured_piece % 6) * 10 - (a.moved_piece % 6))
                   : (a.is_castling ? 50 : 0);
      int sb =
          (b_mv.captured_piece != NO_PIECE)
              ? (100 + (b_mv.captured_piece % 6) * 10 - (b_mv.moved_piece % 6))
              : (b_mv.is_castling ? 50 : 0);
      return sa > sb;
    });

    int32_t best = -100000;
    for (const auto& m : moves) {
      ChessBoard next = apply_move(b, m);
      uint64_t h = next.compute_hash();
      bool repeated = false;
      for (int i = (int)history.size() - 1;
           i >= std::max(0, (int)history.size() - 8); --i) {
        if (history[i] == h) {
          repeated = true;
          break;
        }
      }
      int32_t val =
          repeated ? 0 : -negamax(next, depth - 1, -beta, -alpha, history);
      if (val > best) best = val;
      if (best > alpha) alpha = best;
      if (alpha >= beta) break;
    }
    return best;
  }

  Move select_best_move(const ChessBoard& b, int depth,
                        const std::vector<uint64_t>& history) const {
    std::vector<Move> moves;
    generate_legal_moves(b, moves, false);
    if (moves.empty()) return Move{};

    std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b_mv) {
      int sa = (a.captured_piece != NO_PIECE)
                   ? (100 + (a.captured_piece % 6) * 10)
                   : (a.is_castling ? 50 : 0);
      int sb = (b_mv.captured_piece != NO_PIECE)
                   ? (100 + (b_mv.captured_piece % 6) * 10)
                   : (b_mv.is_castling ? 50 : 0);
      return sa > sb;
    });

    Move best_m = moves[0];
    int32_t best_val = -1000000;

    for (const auto& m : moves) {
      ChessBoard next = apply_move(b, m);
      uint64_t h = next.compute_hash();
      bool repeated = false;
      for (int i = (int)history.size() - 1;
           i >= std::max(0, (int)history.size() - 8); --i) {
        if (history[i] == h) {
          repeated = true;
          break;
        }
      }
      int32_t val =
          repeated ? 0 : -negamax(next, depth - 1, -100000, 100000, history);
      if (val > best_val) {
        best_val = val;
        best_m = m;
      }
    }
    return best_m;
  }

  void export_json(const std::string& filename) const {
    std::ofstream out(filename);
    out << "{\n";
    out << "  \"schema_version\": 2,\n";
    out << "  \"domain\": \"treestep\",\n";
    out << "  \"lado_A\": [\n    " << lado_A[0].to_json(0) << ",\n    "
        << lado_A[1].to_json(0) << ",\n    " << lado_A[2].to_json(0)
        << "\n  ],\n";
    out << "  \"lado_B\": [\n    " << lado_B[0].to_json(0) << ",\n    "
        << lado_B[1].to_json(0) << ",\n    " << lado_B[2].to_json(0)
        << "\n  ]\n";
    out << "}\n";
    out.close();
  }

  bool load_json(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) return false;
    std::stringstream buf;
    buf << in.rdbuf();
    std::string s = buf.str();

    size_t pos_a = s.find("\"lado_A\"");
    if (pos_a == std::string::npos) return false;
    for (int i = 0; i < 3; ++i) {
      lado_A[i].nodes.clear();
      parse_json_tree_recursive(s, pos_a, lado_A[i]);
    }

    size_t pos_b = s.find("\"lado_B\"");
    if (pos_b == std::string::npos) return false;
    for (int i = 0; i < 3; ++i) {
      lado_B[i].nodes.clear();
      parse_json_tree_recursive(s, pos_b, lado_B[i]);
    }
    return true;
  }
};