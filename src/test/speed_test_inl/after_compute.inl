// 结束计时
auto compute_end = std::chrono::high_resolution_clock::now();
auto compute_duration =
    std::chrono::duration_cast<std::chrono::microseconds>(compute_end - compute_start);
double compute_ms = compute_duration.count() / 1000.0;

// 统计结束通信量
vector<uint64_t> compute_bytes_end = {0, 0, 0};
for (int i = 0; i < num_threads; i++) {
    auto tmp = get_netio_bytes(party_id, *netio_list[i]);
    for (int j = 0; j < 3; j++) {
        compute_bytes_end[j] += tmp[j];
    }
}
for (int i = 0; i < NUM_PARTIES; i++) {
    compute_bytes_end[i] -= compute_bytes_start[i];
}