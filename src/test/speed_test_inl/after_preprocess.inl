// 结束计时
auto preprocess_end = std::chrono::high_resolution_clock::now();
auto preprocess_duration =
    std::chrono::duration_cast<std::chrono::microseconds>(preprocess_end - preprocess_start);
double preprocess_ms = preprocess_duration.count() / 1000.0;

// 统计结束通信量
vector<uint64_t> preprocess_bytes_end = {0, 0, 0};
for (int i = 0; i < num_threads; i++) {
    auto tmp = get_netio_bytes(party_id, *netio_list[i]);
    for (int j = 0; j < 3; j++) {
        preprocess_bytes_end[j] += tmp[j];
    }
}
for (int i = 0; i < NUM_PARTIES; i++) {
    preprocess_bytes_end[i] -= preprocess_bytes_start[i];
}