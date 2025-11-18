cout << "Preprocess : " << std::fixed << std::setprecision(2) << std::setw(10) << std::setfill(' ')
     << preprocess_ms << " ms, " << endl;
cout << "Communication bytes: " << endl;
for (int i = 0; i < NUM_PARTIES; i++) {
    if (i != party_id) {
        cout << "P" << party_id << "->P" << i << ": " << std::fixed << std::setw(10)
             << std::setfill(' ') << preprocess_bytes_end[i] << " bytes; " << endl;
    }
}
cout << endl;
cout << "online     : " << std::fixed << std::setprecision(2) << std::setw(10) << std::setfill(' ')
     << compute_ms << " ms" << endl;
cout << "Communication bytes: " << endl;
for (int i = 0; i < NUM_PARTIES; i++) {
    if (i != party_id) {
        cout << "P" << party_id << "->P" << i << ": " << std::fixed << std::setw(10)
             << std::setfill(' ') << compute_bytes_end[i] << " bytes; " << endl;
    }
}