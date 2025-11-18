// set omp threads
omp_set_num_threads(num_threads);

// net io
int party_id = atoi(argv[1]);
NetIOMP *netio_list[num_threads];
for (int t = 0; t < num_threads; t++) {
    netio_list[t] = new NetIOMP(party_id, BASE_PORT + t * 20, NUM_PARTIES);
    netio_list[t]->sync();
}
NetIOMP *netio_main = netio_list[0];

// setup prg
vector<PRGSync> PRGs_list[num_threads];
for (size_t t = 0; t < num_threads; t++) {
    block seed1;
    block seed2;
    if (party_id == 0) {
        seed1 = gen_seed();
        seed2 = gen_seed();
        netio_main->send_data(1, &seed1, sizeof(block));
        netio_main->send_data(2, &seed2, sizeof(block));
    } else if (party_id == 1) {
        netio_main->recv_data(0, &seed1, sizeof(block));
        seed2 = gen_seed();
        netio_main->send_data(2, &seed2, sizeof(block));
    } else if (party_id == 2) {
        netio_main->recv_data(0, &seed1, sizeof(block));
        netio_main->recv_data(1, &seed2, sizeof(block));
    }
    auto private_seed = gen_seed();
    block public_seed;
    if (party_id == 0) {
        public_seed = gen_seed();
        netio_main->send_data(1, &public_seed, sizeof(block));
        netio_main->send_data(2, &public_seed, sizeof(block));
    } else {
        netio_main->recv_data(0, &public_seed, sizeof(block));
    }
    vector<PRGSync> PRGs = {PRGSync(&seed1), PRGSync(&seed2), PRGSync(&private_seed),
                            PRGSync(&public_seed)};
    PRGs_list[t] = PRGs;
}

int vec_len = test_nums;
int batch_size = min(global_batch_size, vec_len / num_threads);