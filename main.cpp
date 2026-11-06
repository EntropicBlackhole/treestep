#include <chrono>
#include <iomanip>
#include <iostream>

#include "uci.hpp"

std::vector<ChessBoard> build_opening_book() {
  std::vector<ChessBoard> book;
  ChessBoard start = make_starting_board();
  book.push_back(start);

  auto play_sq = [](ChessBoard b, int from, int to) {
    int mp = b.piece_at(from);
    int cp = b.piece_at(to);
    return apply_move(b, {(uint8_t)from, (uint8_t)to, (uint8_t)mp, (uint8_t)cp,
                          NO_PIECE, false, false});
  };

  book.push_back(play_sq(play_sq(start, 12, 28), 52, 36));
  book.push_back(play_sq(play_sq(start, 12, 28), 50, 34));
  book.push_back(play_sq(play_sq(start, 11, 27), 51, 35));
  book.push_back(play_sq(play_sq(start, 11, 27), 62, 45));
  book.push_back(play_sq(play_sq(start, 10, 26), 52, 36));
  return book;
}

int32_t play_selfplay_game(const ChessMaestro& white_ai,
                           const ChessMaestro& black_ai,
                           const ChessBoard& start_pos, int search_depth,
                           int max_plies) {
  ChessBoard b = start_pos;
  std::vector<uint64_t> history;
  history.reserve(max_plies + 4);
  history.push_back(b.compute_hash());

  for (int ply = 0; ply < max_plies; ++ply) {
    std::vector<Move> legal;
    generate_legal_moves(b, legal, false);

    if (legal.empty()) {
      if (is_in_check(b, b.white_to_move)) {
        int32_t mate_score = 900 + (max_plies - ply) * 2;
        return b.white_to_move ? -mate_score : mate_score;
      }
      return 0;
    }

    const ChessMaestro& active = b.white_to_move ? white_ai : black_ai;
    Move chosen = active.select_best_move(b, search_depth, history);
    b = apply_move(b, chosen);
    uint64_t h = b.compute_hash();

    int rep = 0;
    for (uint64_t prev_h : history) {
      if (prev_h == h) rep++;
    }
    if (rep >= 2) return 0;
    history.push_back(h);
  }

  int32_t mat_diff = material_score_side(b, 0) - material_score_side(b, 6);
  int32_t pos_diff = std::clamp(CHESS_MARKOV.evaluate(b), -100, 100);
  return std::clamp((mat_diff + pos_diff) / 2, -600, 600);
}

struct MatchStats {
  int64_t net_score = 0;
  int wins = 0;
  int draws = 0;
  int losses = 0;
};

MatchStats play_match_series(const ChessMaestro& candidate,
                             const ChessMaestro& opponent,
                             const std::vector<ChessBoard>& book,
                             int num_openings, int search_depth, int max_plies,
                             int64_t lambda_parsimony) {
  MatchStats st;
  int limit = std::min((int)book.size(), num_openings);

  for (int i = 0; i < limit; ++i) {
    int32_t g1 = play_selfplay_game(candidate, opponent, book[i], search_depth,
                                    max_plies);
    st.net_score += g1;
    if (g1 > 150)
      st.wins++;
    else if (g1 < -150)
      st.losses++;
    else
      st.draws++;

    int32_t g2 = -play_selfplay_game(opponent, candidate, book[i], search_depth,
                                     max_plies);
    st.net_score += g2;
    if (g2 > 150)
      st.wins++;
    else if (g2 < -150)
      st.losses++;
    else
      st.draws++;
  }
  st.net_score -= (lambda_parsimony * candidate.total_nodes());
  return st;
}

int compute_anchor_elo(const MatchStats& vs_anchor) {
  int total_games = vs_anchor.wins + vs_anchor.draws + vs_anchor.losses;
  if (total_games == 0) return 1450;
  double points = vs_anchor.wins + 0.5 * vs_anchor.draws;
  double ratio = std::clamp(points / (double)total_games, 0.05, 0.95);
  double elo_diff = -400.0 * std::log10((1.0 / ratio) - 1.0);
  double margin_bonus =
      std::clamp((double)vs_anchor.net_score / 60.0, -100.0, 150.0);
  return 1450 + (int)std::round(elo_diff + margin_bonus);
}

int main(int argc, char* argv[]) {
  bool train_mode = false;
  int pop_size = 32;
  int generations = 500;
  int search_depth = 2;
  int max_plies = 60;
  uint32_t master_seed = 2026;
  std::string out_file = "treestep_model.json";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--train")
      train_mode = true;
    else if (arg == "--uci")
      train_mode = false;
    else if (arg.rfind("--pop=", 0) == 0) {
      pop_size = std::max(6, std::stoi(arg.substr(6)));
      train_mode = true;
    } else if (arg.rfind("--gens=", 0) == 0) {
      generations = std::max(1, std::stoi(arg.substr(7)));
      train_mode = true;
    } else if (arg.rfind("--depth=", 0) == 0) {
      search_depth = std::clamp(std::stoi(arg.substr(8)), 1, 4);
      train_mode = true;
    } else if (arg.rfind("--plies=", 0) == 0) {
      max_plies = std::max(20, std::stoi(arg.substr(8)));
      train_mode = true;
    } else if (arg.rfind("--seed=", 0) == 0) {
      master_seed = (uint32_t)std::stoul(arg.substr(7));
      train_mode = true;
    } else if (arg.rfind("--out=", 0) == 0) {
      out_file = arg.substr(6);
    }
  }

  if (!train_mode) {
    run_uci_loop(argv[0], out_file);
    return 0;
  }

  std::cout << "[treestep 1.0 trainer | author: EPBH]\n";
  std::cout << "pop=" << pop_size << " | gens=" << generations
            << " | search_depth=" << search_depth
            << " | max_plies=" << max_plies << " | out=" << out_file << "\n\n";

  std::mt19937 master_rng(master_seed);
  std::vector<ChessBoard> book = build_opening_book();

  std::vector<ChessMaestro> population(pop_size);
  for (int p = 0; p < pop_size; ++p) {
    for (int s = 0; s < 3; ++s) {
      if (p < pop_size / 3) {
        population[p].lado_A[s] = create_seeded_byte_specialist(s, false);
        population[p].lado_B[s] = create_seeded_byte_specialist(s, true);
        if (p > 0) {
          mutate_byte_tree(population[p].lado_A[s], master_rng);
          mutate_byte_tree(population[p].lado_B[s], master_rng);
        }
      } else {
        population[p].lado_A[s] = create_random_byte_tree(3, master_rng);
        population[p].lado_B[s] = create_random_byte_tree(3, master_rng);
      }
    }
  }

  const ChessMaestro baseline_anchor = population[0];
  ChessMaestro global_champion = population[0];
  global_champion.export_json(out_file);
  int champ_elo = 1450;
  int64_t best_anchor_score = 0;

  auto train_start_time = std::chrono::high_resolution_clock::now();

  for (int gen = 1; gen <= generations; ++gen) {
    auto gen_start_time = std::chrono::high_resolution_clock::now();

    int opening_idx = (gen - 1) % (int)book.size();
    std::vector<ChessBoard> train_openings = {
        book[opening_idx], book[(opening_idx + 1) % book.size()]};

#pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < pop_size; ++i) {
      auto st =
          play_match_series(population[i], global_champion, train_openings, 2,
                            search_depth, max_plies, 2LL);
      population[i].fitness = st.net_score;
    }

    std::sort(population.begin(), population.end(),
              [](const ChessMaestro& a, const ChessMaestro& b) {
                return a.fitness > b.fitness;
              });

    MatchStats vs_champ =
        play_match_series(population[0], global_champion, book,
                          (int)book.size(), search_depth, max_plies, 2LL);

    auto now_tp = std::chrono::high_resolution_clock::now();
    double gen_sec =
        std::chrono::duration<double>(now_tp - gen_start_time).count();
    double total_sec =
        std::chrono::duration<double>(now_tp - train_start_time).count();

    if (vs_champ.net_score > 150 && vs_champ.wins > vs_champ.losses) {
      MatchStats vs_anchor =
          play_match_series(population[0], baseline_anchor, book,
                            (int)book.size(), search_depth, max_plies, 2LL);
      if (vs_anchor.net_score > 500 && vs_anchor.wins > vs_anchor.losses) {
        global_champion = population[0];
        champ_elo = compute_anchor_elo(vs_anchor);
        global_champion.export_json(out_file);

        now_tp = std::chrono::high_resolution_clock::now();
        gen_sec =
            std::chrono::duration<double>(now_tp - gen_start_time).count();
        total_sec =
            std::chrono::duration<double>(now_tp - train_start_time).count();

        std::cout << "[new champ] gen " << std::setw(4) << gen
                  << " | vs champ W-D-L: " << vs_champ.wins << "-"
                  << vs_champ.draws << "-" << vs_champ.losses << " (+"
                  << vs_champ.net_score << ")"
                  << " | vs anchor: +" << vs_anchor.net_score
                  << " | elo: " << champ_elo
                  << " | nodes: " << global_champion.total_nodes() << " | "
                  << std::fixed << std::setprecision(2) << gen_sec << "s ("
                  << std::setprecision(1) << total_sec << "s)\n";
      }
    }

    if (gen % 10 == 0 || gen == generations) {
      std::cout << "gen " << std::setw(4) << gen
                << " | 4-game: " << std::setw(5) << population[0].fitness
                << " | 12-game: " << std::setw(5) << vs_champ.net_score
                << " | champ elo: " << champ_elo << " | " << std::fixed
                << std::setprecision(2) << gen_sec << "s/gen ("
                << std::setprecision(1) << total_sec << "s)\n";
    }

    std::vector<ChessMaestro> next_gen;
    next_gen.reserve(pop_size);
    next_gen.push_back(global_champion);

    int survivors = pop_size / 2;
    for (int i = 0; i < survivors - 1; ++i) next_gen.push_back(population[i]);

    std::uniform_int_distribution<int> spec_pick(0, 5);
    for (int i = 0; i < pop_size / 5; ++i) {
      ChessMaestro hc = global_champion;
      int t_idx = spec_pick(master_rng);
      if (t_idx < 3)
        mutate_byte_tree(hc.lado_A[t_idx], master_rng);
      else
        mutate_byte_tree(hc.lado_B[t_idx - 3], master_rng);
      next_gen.push_back(hc);
    }

    std::uniform_int_distribution<int> pdist(0, survivors - 1);
    while ((int)next_gen.size() < pop_size) {
      const ChessMaestro& p1 = population[pdist(master_rng)];
      const ChessMaestro& p2 = population[pdist(master_rng)];

      ChessMaestro child = p1;
      int t_idx = spec_pick(master_rng);
      if (t_idx < 3) {
        child.lado_A[t_idx] =
            crossover_byte_tree(p1.lado_A[t_idx], p2.lado_A[t_idx], master_rng);
        mutate_byte_tree(child.lado_A[t_idx], master_rng);
      } else {
        int b_idx = t_idx - 3;
        child.lado_B[b_idx] =
            crossover_byte_tree(p1.lado_B[b_idx], p2.lado_B[b_idx], master_rng);
        mutate_byte_tree(child.lado_B[b_idx], master_rng);
      }
      next_gen.push_back(child);
    }
    population = std::move(next_gen);
  }

  global_champion.export_json(out_file);
  std::cout << "\n[training complete] exported champion to " << out_file
            << "\n";
  return 0;
}