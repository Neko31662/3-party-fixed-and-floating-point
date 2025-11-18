// 统计初始通信量
netio_main->sync();
vector<uint64_t> compute_bytes_start = {0, 0, 0};
for (int i = 0; i < num_threads; i++) {
    auto tmp = get_netio_bytes(party_id, *netio_list[i]);
    for (int j = 0; j < 3; j++) {
        compute_bytes_start[j] += tmp[j];
    }
}

// 开始计时
auto compute_start = std::chrono::high_resolution_clock::now();