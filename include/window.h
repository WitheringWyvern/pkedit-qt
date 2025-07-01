// SPDX-License-Identifier: GPL-3.0
// Copyright (C) 2025 Abdur-Rahman Mansoor

#ifndef QT_WINDOW_H
#define QT_WINDOW_H

#include "pokemon.h"
#include "save.h"

#include <QCheckBox>
#include <QMainWindow>
#include <QTableWidget>

#include <QComboBox>
#include <QSpinBox>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

struct options {
    bool backup_save { true };
    bool allow_illegal_modifications { false };
};

class MainWindow : public QMainWindow {
    Q_OBJECT
    pkmn_save save {};
    options opt {};
    QTableWidget *sel_item_table_widget { nullptr };
    QTableWidget *sel_pkmn_table_widget { nullptr };
    usize sel_pkmn_table_row { 0 };
    item_category sel_item_category { item_category::Pocket };
    pokemon *sel_pkmn { nullptr };
    bool save_loaded = false;
    void open_file();
    void add_pkmn_to_table_widget(QTableWidget *, const pokemon *, int) const;
    void set_pkmn_in_editor(pokemon *);
    void add_item_names_to_combo_box(QComboBox *, item_category) const;
    void update_stats_on_ui(const pokemon *) const;
    void modify_iv(QSpinBox *, pkstat);
    void modify_ev(QSpinBox *, pkstat);
    void block_pkmn_editor_signals(bool) const noexcept;
    void update_pid_on_ui(const pokemon *) const;
    void block_all_signals(bool) const noexcept;
    void set_pkmn_gender_combo_box(const pokemon *) const;
    void update_party_table_widget() const;
    static void reset_spinbox(QSpinBox *);
    static void reset_combo_box(QComboBox *);
    static void reset_line_edit(QLineEdit *);
    static void reset_table_widget(QTableWidget *);
    static void reset_checkbox(QCheckBox *);
    void reset_ui();

  public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() noexcept override;

  private:
    Ui::MainWindow *ui;
};

void show_popup_error(const char *);

#endif // QT_WINDOW_H
