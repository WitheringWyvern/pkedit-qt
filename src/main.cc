// SPDX-License-Identifier: GPL-3.0
// Copyright (C) 2025 Abdur-Rahman Mansoor

#include "init.h"
#include "window.h"

#include <QApplication>
#include <iostream>

int main(int argc, char *argv[])
{
    auto start = std::chrono::high_resolution_clock::now();
    try {
        init_pkedit();
    } catch (const std::exception &e) {
        fprintf(stderr, "Error initializing libpkedit: %s\n", e.what());
        exit(EXIT_FAILURE);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "init_pkedit: took " << elapsed.count() << " seconds" << '\n';

    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle("PKEdit");
    w.show();
    return a.exec();
}
