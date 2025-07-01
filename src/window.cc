// SPDX-License-Identifier: GPL-3.0
// Copyright (C) 2025 Abdur-Rahman Mansoor

#include "window.h"
#include "location.h"
#include "rng.h"
#include "save.h"
#include "trainer.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>

#include <iostream>

#define QFILEDIALOG_FILTER "Save Files (*.sav);;All Files (*)"

enum {
    WINDOW_TAB_WIDGET_TRAINER_INFO = 0,
    WINDOW_TAB_WIDGET_PKMN_PARTY = 1,
    WINDOW_TAB_WIDGET_PKMN_EDITOR = 2,
    WINDOW_TAB_WIDGET_ITEMS = 3,

    PKMN_TABLE_NICKNAME_COL = 0,
    PKMN_TABLE_GENDER_COL = 1,
    PKMN_TABLE_LEVEL_COL = 2,
    PKMN_TABLE_SHINY_COL = 3,
    PKMN_TABLE_EGG_COL = 4,

    ITEM_TABLE_NAME_COL = 0,
    ITEM_TABLE_QUANTITY_COL = 1,

    PKMN_EDITOR_TAB_WIDGET_DESCRIPTION = 0,
    PKMN_EDITOR_TAB_WIDGET_MET_CONDITIONS = 1,
    PKMN_EDITOR_TAB_WIDGET_STATS = 2,
    PKMN_EDITOR_TAB_WIDGET_MOVES = 3,
    PKMN_EDITOR_TAB_WIDGET_TRAINER = 4,

    PKMN_GENDER_COMBOBOX_NA = 0,
    PKMN_GENDER_COMBOBOX_MALE = 1,
    PKMN_GENDER_COMBOBOX_FEMALE = 2,
    PKMN_GENDER_COMBOBOX_GENDERLESS = 3,

    PKMN_STATUS_COMBOBOX_HEALTHY = 0,
    PKMN_STATUS_COMBOBOX_PAR = 1,
    PKMN_STATUS_COMBOBOX_PSN = 2,
    PKMN_STATUS_COMBOBOX_SLP = 3,
    PKMN_STATUS_COMBOBOX_FRZ = 4,
    PKMN_STATUS_COMBOBOX_BRN = 5,
};

static void save_file(const QString &file_name, pkmn_save &save, const options &opt)
{
    if (file_name.isEmpty())
        return;

    save.trainer->save();
    write_pkmn_save_file(file_name.toStdString().c_str(), save, opt.backup_save);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->partyTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->saveLoadedLabel->setStyleSheet("font: 16pt \"Sans Serif\"; color: red;");
    sel_item_table_widget = ui->itemsTableWidget;

    auto get_item_combobox_index = [this](const QString &name) -> usize {
        for (usize i = 0; i < ui->itemNameComboBox->count(); ++i)
            if (name == ui->itemNameComboBox->itemText(i))
                return i;
        throw std::runtime_error("error: unable to find item index");
    };

    connect(ui->actionOpen_File, &QAction::triggered, this, [this] { open_file(); });
    connect(ui->actionBackup_Save, &QAction::triggered, this,
            [this] { opt.backup_save = ui->actionBackup_Save->isChecked(); });
    connect(ui->actionSave_File, &QAction::triggered, this, [this] {
        try {
            if (!save_loaded)
                throw std::runtime_error("Unable to save: no save loaded");

            const QString filename { QFileDialog::getSaveFileName(this, "Save File", "",
                                                                  QFILEDIALOG_FILTER) };
            save_file(filename, save, opt);
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->actionSave_As, &QAction::triggered, this, [this] {
        try {
            if (!save_loaded)
                throw std::runtime_error("Unable to save: no save loaded");

            const QString filename { QFileDialog::getSaveFileName(
                this, "Save As", save.file_name.c_str(), QFILEDIALOG_FILTER) };
            save_file(filename, save, opt);
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->actionAllow_Potentially_Illegal_Modifications, &QAction::triggered, this, [this] {
        opt.allow_illegal_modifications =
            ui->actionAllow_Potentially_Illegal_Modifications->isChecked();

        if (sel_pkmn != nullptr)
            set_pkmn_in_editor(sel_pkmn);
    });
    connect(ui->partyTableWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection &selected, const QItemSelection &deselected) {
                if (ui->partyTableWidget->selectedItems().isEmpty()) {
                    ui->editPkmnPartyPushButton->setEnabled(false);
                    ui->deletePkmnPartyPushButton->setEnabled(false);
                }
            });
    connect(ui->partyTableWidget, &QTableWidget::itemClicked, this, [this](QTableWidgetItem *sel) {
        if (sel->row() >= save.trainer->pkmn_team().size())
            return;
        ui->editPkmnPartyPushButton->setEnabled(true);
        ui->deletePkmnPartyPushButton->setEnabled(save.trainer->pkmn_team().size() > 1);
    });
    connect(ui->editPkmnPartyPushButton, &QPushButton::clicked, this, [this]() {
        const int row = ui->partyTableWidget->selectedItems()[0]->row();
        sel_pkmn_table_widget = ui->partyTableWidget;
        sel_pkmn_table_row = row;
        set_pkmn_in_editor(save.trainer->pkmn_team()[row].get());
    });
    connect(ui->deletePkmnPartyPushButton, &QPushButton::clicked, this, [this]() {
        try {
            if (save.trainer->pkmn_team().size() <= 1)
                throw std::runtime_error("Cannot delete last pokemon in party");

            save.trainer->remove_pkmn_from_party(ui->partyTableWidget->selectedItems()[0]->row());
            ui->partyTableWidget->clearSelection();
            ui->editPkmnPartyPushButton->setEnabled(false);
            ui->deletePkmnPartyPushButton->setEnabled(false);
            update_party_table_widget();
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->itemsTabWidget, &QTabWidget::currentChanged, this,
            [this, get_item_combobox_index](int index) {
                switch (index) {
                    default:
                        qDebug() << "Invalid item tab widget index";
                        break;
                    case 0:
                        sel_item_table_widget = ui->itemsTableWidget;
                        sel_item_category = item_category::Pocket;
                        break;
                    case 1:
                        sel_item_table_widget = ui->ballsTableWidget;
                        sel_item_category = item_category::Pokeball;
                        break;
                    case 2:
                        sel_item_table_widget = ui->berriesTableWidget;
                        sel_item_category = item_category::Berry;
                        break;
                    case 3:
                        sel_item_table_widget = ui->tmsTableWidget;
                        sel_item_category = item_category::Tm;
                        break;
                    case 4:
                        sel_item_table_widget = ui->keyItemsTableWidget;
                        sel_item_category = item_category::Key_Item;
                        break;
                    case 5:
                        sel_item_table_widget = ui->pcItemsTableWidget;
                        sel_item_category = item_category::Pc;
                        break;
                }

                add_item_names_to_combo_box(ui->itemNameComboBox, sel_item_category);
                const bool selected = !sel_item_table_widget->selectedItems().isEmpty();
                ui->editItemPushButton->setEnabled(selected);
                ui->deleteItemPushButton->setEnabled(selected);
                if (selected) {
                    ui->itemNameComboBox->setCurrentIndex(
                        get_item_combobox_index(sel_item_table_widget->currentItem()->text()));
                    ui->quantitySpinBox->setValue(
                        sel_item_table_widget->currentItem()->text().toInt());
                } else {
                    ui->itemNameComboBox->setCurrentIndex(0);
                    ui->quantitySpinBox->setValue(0);
                }
            });

    auto on_item_select = [this, get_item_combobox_index](QTableWidgetItem *) {
        const QString name { sel_item_table_widget
                                 ->item(sel_item_table_widget->currentRow(), ITEM_TABLE_NAME_COL)
                                 ->text() };
        const u16 quantity =
            sel_item_table_widget
                ->item(sel_item_table_widget->currentRow(), ITEM_TABLE_QUANTITY_COL)
                ->text()
                .toUInt();

        ui->itemNameComboBox->clear();
        add_item_names_to_combo_box(ui->itemNameComboBox, sel_item_category);
        ui->itemNameComboBox->setCurrentIndex(get_item_combobox_index(name));
        ui->quantitySpinBox->setValue(quantity);
        ui->editItemPushButton->setEnabled(true);
        ui->deleteItemPushButton->setEnabled(true);
    };

    connect(ui->itemsTableWidget, &QTableWidget::itemClicked, this, on_item_select);
    connect(ui->ballsTableWidget, &QTableWidget::itemClicked, this, on_item_select);
    connect(ui->berriesTableWidget, &QTableWidget::itemClicked, this, on_item_select);
    connect(ui->tmsTableWidget, &QTableWidget::itemClicked, this, on_item_select);
    connect(ui->keyItemsTableWidget, &QTableWidget::itemClicked, this, on_item_select);
    connect(ui->pcItemsTableWidget, &QTableWidget::itemClicked, this, on_item_select);
    connect(ui->addItemPushButton, &QPushButton::clicked, this, [this] {
        const QString name { ui->itemNameComboBox->currentText() };
        const u16 quantity = ui->quantitySpinBox->value();

        try {
            save.trainer->add_item(sel_item_category, name.toStdString().c_str(), quantity);
            sel_item_table_widget->insertRow(sel_item_table_widget->rowCount());
            sel_item_table_widget->setItem(sel_item_table_widget->rowCount() - 1,
                                           ITEM_TABLE_NAME_COL, new QTableWidgetItem(name));
            sel_item_table_widget->setItem(sel_item_table_widget->rowCount() - 1,
                                           ITEM_TABLE_QUANTITY_COL,
                                           new QTableWidgetItem(std::to_string(quantity).c_str()));
        } catch (std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->editItemPushButton, &QPushButton::clicked, this, [this] {
        const u16 row = sel_item_table_widget->currentRow();
        const QString name { ui->itemNameComboBox->currentText() };
        const u16 quantity = ui->quantitySpinBox->value();
        save.trainer->edit_item(sel_item_category, row, name.toStdString().c_str(), quantity);
        sel_item_table_widget->removeRow(row);
        sel_item_table_widget->insertRow(row);
        sel_item_table_widget->setItem(row, ITEM_TABLE_NAME_COL, new QTableWidgetItem(name));
        sel_item_table_widget->setItem(row, ITEM_TABLE_QUANTITY_COL,
                                       new QTableWidgetItem(std::to_string(quantity).c_str()));
        sel_item_table_widget->setCurrentItem(sel_item_table_widget->item(row, 0));
    });
    connect(ui->deleteItemPushButton, &QPushButton::clicked, this, [this] {
        const u16 row = sel_item_table_widget->currentRow();
        save.trainer->del_item(sel_item_category, row);
        sel_item_table_widget->removeRow(row);
    });
    connect(ui->hpIvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_iv(ui->hpIvSpinBox, pkstat::Hp); });
    connect(ui->atkIvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_iv(ui->atkIvSpinBox, pkstat::Atk); });
    connect(ui->defIvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_iv(ui->defIvSpinBox, pkstat::Def); });
    connect(ui->speIvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_iv(ui->speIvSpinBox, pkstat::Spe); });
    connect(ui->spAtkIvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_iv(ui->spAtkIvSpinBox, pkstat::Spa); });
    connect(ui->spDefIvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_iv(ui->spDefIvSpinBox, pkstat::Spd); });
    connect(ui->spDvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_iv(ui->spDvSpinBox, pkstat::Spe); });
    connect(ui->spDvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_iv(ui->spDvSpinBox, pkstat::Spe); });
    connect(ui->hpevSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_ev(ui->hpevSpinBox, pkstat::Hp); });
    connect(ui->atkEvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_ev(ui->atkEvSpinBox, pkstat::Atk); });
    connect(ui->defEvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_ev(ui->defEvSpinBox, pkstat::Def); });
    connect(ui->speEvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_ev(ui->speEvSpinBox, pkstat::Spe); });
    connect(ui->spAtkEvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_ev(ui->spAtkEvSpinBox, pkstat::Spa); });
    connect(ui->spDefEvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_ev(ui->spDefEvSpinBox, pkstat::Spd); });
    connect(ui->spcEvSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this] { modify_ev(ui->spcEvSpinBox, pkstat::Spe); });
    connect(ui->levelSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        try {
            if (sel_pkmn != nullptr) {
                sel_pkmn->set_level(ui->levelSpinBox->value());
                ui->expSpinBox->blockSignals(true);
                ui->expSpinBox->setMinimum(sel_pkmn->min_exp());
                ui->expSpinBox->setValue(sel_pkmn->exp());
                ui->expSpinBox->setMaximum(sel_pkmn->max_exp());
                ui->expSpinBox->blockSignals(false);
                update_stats_on_ui(sel_pkmn);
                if (sel_pkmn_table_widget == ui->partyTableWidget)
                    update_party_table_widget();
            }
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->nameLineEdit, &QLineEdit::textChanged, this, [this] {
        try {
            save.trainer->set_name(ui->nameLineEdit->text().toStdWString());
            ui->nameLineEdit->setText(QString::fromStdWString(save.trainer->name()));
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->genderComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        try {
            save.trainer->set_gender(ui->genderComboBox->currentIndex());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->moneySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        try {
            save.trainer->set_money(ui->moneySpinBox->value());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->coinsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        try {
            save.trainer->set_coins(ui->coinsSpinBox->value());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->speciesComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_species(ui->speciesComboBox->currentIndex());
            set_pkmn_in_editor(sel_pkmn);
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->nicknameLineEdit, &QLineEdit::textChanged, this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            if (ui->nicknameLineEdit->text().isEmpty())
                return;
            sel_pkmn->set_nickname(ui->nicknameLineEdit->text().toStdWString());
            if (sel_pkmn_table_widget == ui->partyTableWidget)
                update_party_table_widget();
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->expSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_exp(ui->expSpinBox->value());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->friendshipSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_friendship(ui->friendshipSpinBox->value());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->pkmnGenderComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] {
                if (sel_pkmn == nullptr)
                    return;

                try {
                    pokemon_gender gender {};
                    switch (ui->pkmnGenderComboBox->currentIndex()) {
                        default:
                            throw std::runtime_error("Invalid gender");
                        case PKMN_GENDER_COMBOBOX_NA:
                            gender = pokemon_gender::NA;
                            break;
                        case PKMN_GENDER_COMBOBOX_MALE:
                            gender = pokemon_gender::MALE;
                            break;
                        case PKMN_GENDER_COMBOBOX_FEMALE:
                            gender = pokemon_gender::FEMALE;
                            break;
                        case PKMN_GENDER_COMBOBOX_GENDERLESS:
                            gender = pokemon_gender::GENDERLESS;
                            break;
                    }
                    sel_pkmn->set_gender(gender);
                    ui->shinyCheckBox->blockSignals(true);
                    ui->natureComboBox->blockSignals(true);
                    ui->shinyCheckBox->setChecked(sel_pkmn->is_shiny());
                    ui->natureComboBox->setCurrentIndex(sel_pkmn->nature() + 1);
                    ui->shinyCheckBox->blockSignals(false);
                    ui->natureComboBox->blockSignals(false);
                    update_pid_on_ui(sel_pkmn);
                    if (sel_pkmn_table_widget == ui->partyTableWidget)
                        update_party_table_widget();
                } catch (const std::exception &e) {
                    show_popup_error(e.what());
                }
            });
    connect(ui->natureComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            if (ui->natureComboBox->currentIndex() == 0)
                throw std::runtime_error("Invalid nature");

            sel_pkmn->set_nature(static_cast<pkmn_nature>(ui->natureComboBox->currentIndex() - 1));
            ui->pkmnGenderComboBox->blockSignals(true);
            ui->shinyCheckBox->blockSignals(true);
            set_pkmn_gender_combo_box(sel_pkmn);
            ui->shinyCheckBox->setChecked(sel_pkmn->is_shiny());
            ui->pkmnGenderComboBox->blockSignals(false);
            ui->shinyCheckBox->blockSignals(false);
            update_pid_on_ui(sel_pkmn);
            update_stats_on_ui(sel_pkmn);
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->statusComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            status_condition status {};
            switch (ui->statusComboBox->currentIndex()) {
                case PKMN_STATUS_COMBOBOX_HEALTHY:
                    status = status_condition::HEALTHY;
                    break;
                case PKMN_STATUS_COMBOBOX_BRN:
                    status = status_condition::BRN;
                    break;
                case PKMN_STATUS_COMBOBOX_FRZ:
                    status = status_condition::FRZ;
                    break;
                case PKMN_STATUS_COMBOBOX_PAR:
                    status = status_condition::PAR;
                    break;
                case PKMN_STATUS_COMBOBOX_SLP:
                    status = status_condition::SLP;
                    break;
                case PKMN_STATUS_COMBOBOX_PSN:
                    status = status_condition::PSN;
                    break;
            }

            sel_pkmn->set_status(status);
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->abilityComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_ability(ui->abilityComboBox->currentIndex());
            update_pid_on_ui(sel_pkmn);
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->infectedCheckBox, QOverload<int>::of(&QCheckBox::stateChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_infected(ui->infectedCheckBox->isChecked());
            ui->curedCheckBox->blockSignals(true);
            ui->curedCheckBox->setChecked(sel_pkmn->is_cured());
            ui->curedCheckBox->blockSignals(false);
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->curedCheckBox, QOverload<int>::of(&QCheckBox::stateChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_cured(ui->curedCheckBox->isChecked());
            ui->infectedCheckBox->blockSignals(true);
            ui->infectedCheckBox->setChecked(sel_pkmn->is_infected());
            ui->infectedCheckBox->blockSignals(false);
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->heldItemComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] {
                if (sel_pkmn == nullptr)
                    return;

                try {
                    sel_pkmn->set_held_item(save.trainer->item_idx_from_name(
                        ui->heldItemComboBox->currentText().toStdString().c_str()));
                } catch (const std::exception &e) {
                    show_popup_error(e.what());
                }
            });

    connect(ui->shinyCheckBox, QOverload<int>::of(&QCheckBox::stateChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_shiny(ui->shinyCheckBox->isChecked());
            ui->natureComboBox->blockSignals(true);
            ui->pkmnGenderComboBox->blockSignals(true);
            ui->natureComboBox->setCurrentIndex(sel_pkmn->nature() + 1);
            set_pkmn_gender_combo_box(sel_pkmn);
            ui->pkmnGenderComboBox->blockSignals(false);
            ui->natureComboBox->blockSignals(false);
            update_pid_on_ui(sel_pkmn);
            if (sel_pkmn_table_widget == ui->partyTableWidget)
                update_party_table_widget();
        } catch (const std::exception &e) {
            ui->shinyCheckBox->setChecked(ui->shinyCheckBox->isChecked());
            show_popup_error(e.what());
        }
    });

    connect(ui->eggCheckBox, QOverload<int>::of(&QCheckBox::stateChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_egg(ui->eggCheckBox->isChecked());
            if (sel_pkmn_table_widget == ui->partyTableWidget)
                update_party_table_widget();
        } catch (const std::exception &e) {
            ui->eggCheckBox->setChecked(ui->eggCheckBox->isChecked());
            show_popup_error(e.what());
        }
    });

    connect(ui->originGameComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] {
                if (sel_pkmn == nullptr)
                    return;

                try {
                    sel_pkmn->set_origin_game(ui->originGameComboBox->currentIndex());
                } catch (const std::exception &e) {
                    ui->originGameComboBox->setCurrentIndex(sel_pkmn->game_of_origin());
                    show_popup_error(e.what());
                }
            });

    connect(ui->locationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] {
                if (sel_pkmn == nullptr)
                    return;

                try {
                    sel_pkmn->set_location_met(
                        ui->locationComboBox->currentText().toStdString().c_str());
                } catch (const std::exception &e) {
                    show_popup_error(e.what());
                }
            });

    connect(ui->pokeballComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] {
                if (sel_pkmn == nullptr)
                    return;

                try {
                    sel_pkmn->set_pokeball(ui->pokeballComboBox->currentIndex());
                } catch (const std::exception &e) {
                    show_popup_error(e.what());
                }
            });

    connect(ui->levelMetSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_level_met(ui->levelMetSpinBox->value());
        } catch (const std::exception &e) {
            ui->levelMetSpinBox->setValue(sel_pkmn->level_met());
            show_popup_error(e.what());
        }
    });

    connect(ui->fatefulEncounterCheckBox, QOverload<int>::of(&QCheckBox::stateChanged), this,
            [this] {
                if (sel_pkmn == nullptr)
                    return;

                try {
                    sel_pkmn->set_fateful_encounter(ui->fatefulEncounterCheckBox->isChecked());
                } catch (const std::exception &e) {
                    ui->fatefulEncounterCheckBox->setChecked(sel_pkmn->fateful_encounter());
                    show_popup_error(e.what());
                }
            });

    connect(ui->m1ComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move1(ui->m1ComboBox->currentIndex());
            ui->pp1SpinBox->blockSignals(true);
            ui->pp1SpinBox->setMaximum(sel_pkmn->move1_max_pp());
            ui->pp1SpinBox->setValue(sel_pkmn->pp1());
            ui->pp1SpinBox->blockSignals(false);
            ui->m1MaxppLineEdit->setText(std::to_string(sel_pkmn->move1_max_pp()).c_str());
        } catch (const std::exception &e) {
            ui->m1ComboBox->setCurrentIndex(sel_pkmn->move1());
            show_popup_error(e.what());
        }
    });

    connect(ui->m2ComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move2(ui->m2ComboBox->currentIndex());
            ui->pp2SpinBox->blockSignals(true);
            ui->pp2SpinBox->setMaximum(sel_pkmn->move2_max_pp());
            ui->pp2SpinBox->setValue(sel_pkmn->pp2());
            ui->pp2SpinBox->blockSignals(false);
            ui->m2MaxppLineEdit->setText(std::to_string(sel_pkmn->move2_max_pp()).c_str());
        } catch (const std::exception &e) {
            ui->m2ComboBox->setCurrentIndex(sel_pkmn->move2());
            show_popup_error(e.what());
        }
    });

    connect(ui->m3ComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move3(ui->m3ComboBox->currentIndex());
            ui->pp3SpinBox->blockSignals(true);
            ui->pp3SpinBox->setMaximum(sel_pkmn->move3_max_pp());
            ui->pp3SpinBox->setValue(sel_pkmn->pp3());
            ui->pp3SpinBox->blockSignals(false);
            ui->m3MaxppLineEdit->setText(std::to_string(sel_pkmn->move3_max_pp()).c_str());
        } catch (const std::exception &e) {
            ui->m3ComboBox->setCurrentIndex(sel_pkmn->move3());
            show_popup_error(e.what());
        }
    });

    connect(ui->m4ComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move4(ui->m4ComboBox->currentIndex());
            ui->pp4SpinBox->blockSignals(true);
            ui->pp4SpinBox->setMaximum(sel_pkmn->move4_max_pp());
            ui->pp4SpinBox->setValue(sel_pkmn->pp4());
            ui->pp4SpinBox->blockSignals(false);
            ui->m4MaxppLineEdit->setText(std::to_string(sel_pkmn->move4_max_pp()).c_str());
        } catch (const std::exception &e) {
            ui->m4ComboBox->setCurrentIndex(sel_pkmn->move4());
            show_popup_error(e.what());
        }
    });

    connect(ui->pp1SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move1_pp(ui->pp1SpinBox->value());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->pp2SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move2_pp(ui->pp2SpinBox->value());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->pp3SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move3_pp(ui->pp3SpinBox->value());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->pp4SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move4_pp(ui->pp4SpinBox->value());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->pp1BonusSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move1_bonus(ui->pp1BonusSpinBox->value());
            ui->pp1SpinBox->blockSignals(true);
            ui->pp1SpinBox->setMaximum(sel_pkmn->move1_max_pp());
            ui->pp1SpinBox->setValue(sel_pkmn->pp1());
            ui->pp1SpinBox->blockSignals(false);
            ui->m1MaxppLineEdit->setText(std::to_string(sel_pkmn->move1_max_pp()).c_str());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->pp2BonusSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move2_bonus(ui->pp2BonusSpinBox->value());
            ui->pp2SpinBox->blockSignals(true);
            ui->pp2SpinBox->setMaximum(sel_pkmn->move2_max_pp());
            ui->pp2SpinBox->setValue(sel_pkmn->pp2());
            ui->pp2SpinBox->blockSignals(false);
            ui->m2MaxppLineEdit->setText(std::to_string(sel_pkmn->move2_max_pp()).c_str());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->pp3BonusSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move3_bonus(ui->pp3BonusSpinBox->value());
            ui->pp3SpinBox->blockSignals(true);
            ui->pp3SpinBox->setMaximum(sel_pkmn->move3_max_pp());
            ui->pp3SpinBox->setValue(sel_pkmn->pp3());
            ui->pp3SpinBox->blockSignals(false);
            ui->m3MaxppLineEdit->setText(std::to_string(sel_pkmn->move3_max_pp()).c_str());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->pp4BonusSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            sel_pkmn->set_move4_bonus(ui->pp4BonusSpinBox->value());
            ui->pp4SpinBox->blockSignals(true);
            ui->pp4SpinBox->setMaximum(sel_pkmn->move4_max_pp());
            ui->pp4SpinBox->setValue(sel_pkmn->pp4());
            ui->pp4SpinBox->blockSignals(false);
            ui->m4MaxppLineEdit->setText(std::to_string(sel_pkmn->move4_max_pp()).c_str());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->otPidLineEdit, &QLineEdit::textChanged, this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            if (ui->otPidLineEdit->text().isEmpty())
                return;

            sel_pkmn->set_ot_pid(std::stoi(ui->otPidLineEdit->text().toStdString()));
            ui->shinyCheckBox->blockSignals(true);
            ui->shinyCheckBox->setChecked(sel_pkmn->is_shiny());
            ui->shinyCheckBox->blockSignals(false);
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->otSidLineEdit, &QLineEdit::textChanged, this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            if (ui->otSidLineEdit->text().isEmpty())
                return;

            sel_pkmn->set_ot_sid(std::stoi(ui->otSidLineEdit->text().toStdString()));
            ui->shinyCheckBox->blockSignals(true);
            ui->shinyCheckBox->setChecked(sel_pkmn->is_shiny());
            ui->shinyCheckBox->blockSignals(false);
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    connect(ui->otNameLineEdit, &QLineEdit::textChanged, this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            if (ui->otNameLineEdit->text().isEmpty())
                return;

            sel_pkmn->set_ot_name(ui->otNameLineEdit->text().toStdWString());
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->otGenderComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] {
                if (sel_pkmn == nullptr)
                    return;

                try {
                    sel_pkmn->set_ot_gender(ui->otGenderComboBox->currentIndex());
                } catch (const std::exception &e) {
                    show_popup_error(e.what());
                }
            });
    connect(ui->publicIdLineEdit, &QLineEdit::textChanged, this, [this] {
        try {
            if (ui->publicIdLineEdit->text().isEmpty())
                return;

            save.trainer->set_public_id(std::stoi(ui->publicIdLineEdit->text().toStdString()));
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
    connect(ui->secretIdLineEdit, &QLineEdit::textChanged, this, [this] {
        try {
            if (ui->secretIdLineEdit->text().isEmpty())
                return;

            save.trainer->set_secret_id(std::stoi(ui->secretIdLineEdit->text().toStdString()));
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });

    QIntValidator *pid_sid_validator = new QIntValidator(this);
    pid_sid_validator->setRange(0, std::numeric_limits<u16>::max());
    ui->otPidLineEdit->setValidator(pid_sid_validator);
    ui->otSidLineEdit->setValidator(pid_sid_validator);
    ui->publicIdLineEdit->setValidator(pid_sid_validator);
    ui->secretIdLineEdit->setValidator(pid_sid_validator);

    connect(ui->pkmnSimulateTradePushButton, &QPushButton::clicked, this, [this] {
        if (sel_pkmn == nullptr)
            return;

        try {
            if (!sel_pkmn->has_trade_evolution())
                throw std::runtime_error("Pokemon does not have a trade evolution");

            sel_pkmn->simulate_trade_evolution();
            ui->speciesComboBox->blockSignals(true);
            ui->nicknameLineEdit->blockSignals(true);
            ui->speciesComboBox->setCurrentIndex(sel_pkmn->species());
            ui->nicknameLineEdit->setText(QString::fromStdWString(sel_pkmn->nickname()));
            ui->nicknameLineEdit->blockSignals(false);
            ui->speciesComboBox->blockSignals(false);
            update_stats_on_ui(sel_pkmn);
            ui->pkmnSimulateTradePushButton->setEnabled(false);
        } catch (const std::exception &e) {
            show_popup_error(e.what());
        }
    });
}

MainWindow::~MainWindow() noexcept { delete ui; }

void MainWindow::open_file()
{
    QString filename { QFileDialog::getOpenFileName(nullptr, "Open File", "", QFILEDIALOG_FILTER) };

    if (filename.isEmpty())
        return;

    try {
        if (save_loaded) {
            reset_ui();
            delete save.trainer;
            save_loaded = false;
        }

        save = read_pkmn_save_file(filename.toStdString().c_str());
        block_all_signals(true);
        save_loaded = true;
        ui->saveLoadedLabel->setText(
            (std::string { "Detected Save: Pokemon " } + save.game_name).data());
        ui->saveLoadedLabel->setStyleSheet("font: 16pt \"Sans Serif\"; color: green;");
        ui->nameLineEdit->setEnabled(true);
        ui->genderComboBox->setEnabled(true);
        ui->moneySpinBox->setEnabled(true);
        ui->coinsSpinBox->setEnabled(true);
        ui->partyTableWidget->setEnabled(true);
        ui->addItemPushButton->setEnabled(true);
        ui->nameLineEdit->setText(QString::fromStdWString(save.trainer->name()));
        ui->genderComboBox->setCurrentIndex(save.trainer->is_female());

        ui->nameLineEdit->setMaxLength(save.trainer->name_length());
        ui->coinsSpinBox->setMaximum(save.trainer->max_coins());
        ui->moneySpinBox->setMaximum(save.trainer->max_money());

        ui->moneySpinBox->setValue(save.trainer->money());
        ui->coinsSpinBox->setValue(save.trainer->coins());
        ui->publicIdLineEdit->setText(
            QString::fromStdString(std::to_string(save.trainer->public_id())));
        ui->secretIdLineEdit->setText(
            QString::fromStdString(std::to_string(save.trainer->secret_id())));
        ui->publicIdLineEdit->setEnabled(true);
        ui->secretIdLineEdit->setEnabled(true);

        const trainer_time_played tm { save.trainer->time_played() };
        ui->timePlayedLineEdit->setText(QString::fromStdString(std::to_string(tm.hours) + ":" +
                                                               std::to_string(tm.minutes) + ":" +
                                                               std::to_string(tm.seconds)));

        update_party_table_widget();

        auto add_items = [](const std::vector<std::shared_ptr<item>> &items, QTableWidget *table) {
            for (usize i = 0; i < items.size(); ++i) {
                QTableWidgetItem *name = new QTableWidgetItem(items[i]->name());
                QTableWidgetItem *quantity =
                    new QTableWidgetItem(std::to_string(items[i]->count()).c_str());
                table->insertRow(i);
                table->setItem(i, ITEM_TABLE_NAME_COL, name);
                table->setItem(i, ITEM_TABLE_QUANTITY_COL, quantity);
            }
        };

        add_items(save.trainer->get_pocket_items(), ui->itemsTableWidget);
        add_items(save.trainer->get_ball_items(), ui->ballsTableWidget);
        add_items(save.trainer->get_berry_case(), ui->berriesTableWidget);
        add_items(save.trainer->get_tm_case(), ui->tmsTableWidget);
        add_items(save.trainer->get_key_items(), ui->keyItemsTableWidget);
        add_items(save.trainer->get_pc_items(), ui->pcItemsTableWidget);

        ui->itemsTabWidget->setEnabled(true);
        ui->itemsTableWidget->setEnabled(true);
        ui->ballsTableWidget->setEnabled(true);
        ui->berriesTableWidget->setEnabled(true);
        ui->tmsTableWidget->setEnabled(true);
        ui->keyItemsTableWidget->setEnabled(true);
        ui->pcItemsTableWidget->setEnabled(true);

        add_item_names_to_combo_box(ui->itemNameComboBox, sel_item_category);

        ui->itemNameComboBox->setEnabled(true);
        ui->quantitySpinBox->setEnabled(true);

        block_all_signals(false);
    } catch (std::exception &e) {
        show_popup_error(e.what());
        block_all_signals(false);
    }
}

void MainWindow::add_pkmn_to_table_widget(QTableWidget *table, const pokemon *pkmn, int index) const
{
    QTableWidgetItem *nickname = new QTableWidgetItem(QString::fromStdWString(pkmn->nickname()));
    QTableWidgetItem *gender = new QTableWidgetItem(pkmn->gender_name());
    QTableWidgetItem *level = new QTableWidgetItem(std::to_string(pkmn->level()).c_str());
    QTableWidgetItem *shiny = new QTableWidgetItem(pkmn->is_shiny() ? "Yes" : "No");
    QTableWidgetItem *egg = new QTableWidgetItem(pkmn->is_egg() ? "Yes" : "No");
    table->setItem(index, PKMN_TABLE_NICKNAME_COL, nickname);
    table->setItem(index, PKMN_TABLE_GENDER_COL, gender);
    table->setItem(index, PKMN_TABLE_LEVEL_COL, level);
    table->setItem(index, PKMN_TABLE_SHINY_COL, shiny);
    table->setItem(index, PKMN_TABLE_EGG_COL, egg);
}

void MainWindow::modify_iv(QSpinBox *spin_box, pkstat stat)
{
    if (sel_pkmn == nullptr)
        return;

    const u8 iv = spin_box->value();
    try {
        sel_pkmn->set_iv(stat, iv);
        update_stats_on_ui(sel_pkmn);
    } catch (std::exception &e) {
        show_popup_error(e.what());
    }
}

void MainWindow::modify_ev(QSpinBox *spin_box, pkstat stat)
{
    if (sel_pkmn == nullptr)
        return;

    const u8 ev = spin_box->value();
    try {
        sel_pkmn->set_ev(stat, ev);
    } catch (std::exception &e) {
        show_popup_error(e.what());
    }
    update_stats_on_ui(sel_pkmn);
}

void MainWindow::update_stats_on_ui(const pokemon *pkmn) const
{
    if (pkmn == nullptr)
        return;

    ui->hpSpinBox->setValue(pkmn->total_hp());
    ui->atkSpinBox->setValue(pkmn->attack());
    ui->defSpinBox->setValue(pkmn->defense());
    ui->speSpinBox->setValue(pkmn->speed());

    if (!pkmn->compat_has_spc_eviv()) {
        ui->spAtkSpinBox->setValue(pkmn->special_atk());
        ui->spDefSpinBox->setValue(pkmn->special_def());
        ui->spAtkIvSpinBox->setValue(pkmn->special_atk_iv());
        ui->spDefIvSpinBox->setValue(pkmn->special_def_iv());
        ui->spAtkEvSpinBox->setValue(pkmn->special_atk_ev());
        ui->spDefEvSpinBox->setValue(pkmn->special_def_ev());
    } else {
        if (pkmn->compat_has_spc())
            ui->spSpinBox->setValue(pkmn->special());
        ui->spDvSpinBox->setValue(pkmn->special_dv());
        ui->spcEvSpinBox->setValue(pkmn->special_ev());
    }

    ui->hpIvSpinBox->setValue(pkmn->hp_iv());
    ui->atkIvSpinBox->setValue(pkmn->attack_iv());
    ui->defIvSpinBox->setValue(pkmn->defense_iv());
    ui->speIvSpinBox->setValue(pkmn->speed_iv());

    ui->hpevSpinBox->setValue(pkmn->hp_ev());
    ui->atkEvSpinBox->setValue(pkmn->attack_ev());
    ui->defEvSpinBox->setValue(pkmn->defense_ev());
    ui->speEvSpinBox->setValue(pkmn->speed_ev());
}

void MainWindow::set_pkmn_in_editor(pokemon *pkmn)
{
    block_pkmn_editor_signals(true);

    reset_combo_box(ui->speciesComboBox);
    reset_combo_box(ui->abilityComboBox);
    reset_combo_box(ui->heldItemComboBox);
    reset_combo_box(ui->originGameComboBox);
    reset_combo_box(ui->locationComboBox);
    reset_combo_box(ui->pokeballComboBox);
    reset_combo_box(ui->m1ComboBox);
    reset_combo_box(ui->m2ComboBox);
    reset_combo_box(ui->m3ComboBox);
    reset_combo_box(ui->m4ComboBox);

    if (pkmn == nullptr)
        return;

    // TODO: Handle errors later

    pkmn->allow_illegal_changes(opt.allow_illegal_modifications);
    const pkmn_allowed_set_fields *allow = pkmn->allowed_modifications();

    auto all_species { pkmn->species_list() };
    for (const auto &species : all_species)
        ui->speciesComboBox->addItem(species->name());
    ui->speciesComboBox->setCurrentIndex(pkmn->species());
    ui->speciesComboBox->setEditable(allow->set_species | opt.allow_illegal_modifications);
    ui->speciesComboBox->setEnabled(allow->set_species | opt.allow_illegal_modifications);

    ui->nicknameLineEdit->setText(QString::fromStdWString(pkmn->nickname()));
    ui->nicknameLineEdit->setMaxLength(pkmn->nickname_max_size());
    ui->nicknameLineEdit->setEnabled(true);
    ui->levelSpinBox->setValue(pkmn->level());
    ui->levelSpinBox->setEnabled(true);
    ui->expSpinBox->setValue(pkmn->exp());
    ui->expSpinBox->setMinimum(pkmn->min_exp());
    ui->expSpinBox->setMaximum(pkmn->max_exp());
    ui->expSpinBox->setEnabled(true);
    ui->friendshipSpinBox->setValue(pkmn->friendship());
    ui->friendshipSpinBox->setEnabled(true);
    update_pid_on_ui(pkmn);

    u8 index;
    if (pkmn->compat_has_gender()) {
        set_pkmn_gender_combo_box(pkmn);
        ui->pkmnGenderComboBox->setEnabled(allow->set_gender | opt.allow_illegal_modifications);
        ui->pkmnGenderComboBox->setEditable(allow->set_gender | opt.allow_illegal_modifications);
    } else {
        ui->pkmnGenderComboBox->setEnabled(false);
    }

    if (pkmn->compat_has_nature()) {
        ui->natureComboBox->setCurrentIndex(pkmn->nature() + 1);
        ui->natureComboBox->setEnabled(allow->set_nature | opt.allow_illegal_modifications);
        ui->natureComboBox->setEditable(allow->set_nature | opt.allow_illegal_modifications);
    } else {
        ui->natureComboBox->setEnabled(false);
    }

    ui->tabWidget->setCurrentIndex(WINDOW_TAB_WIDGET_PKMN_EDITOR);
    ui->pkmnEditorTabWidget->setCurrentIndex(PKMN_EDITOR_TAB_WIDGET_DESCRIPTION);

    switch (pkmn->status()) {
        default:
            throw std::runtime_error("invalid status condition");
        case status_condition::HEALTHY:
            index = PKMN_STATUS_COMBOBOX_HEALTHY;
            break;
        case status_condition::PAR:
            index = PKMN_STATUS_COMBOBOX_PAR;
            break;
        case status_condition::PSN:
            index = PKMN_STATUS_COMBOBOX_PSN;
            break;
        case status_condition::SLP:
            index = PKMN_STATUS_COMBOBOX_SLP;
            break;
        case status_condition::FRZ:
            index = PKMN_STATUS_COMBOBOX_FRZ;
            break;
        case status_condition::BRN:
            index = PKMN_STATUS_COMBOBOX_BRN;
            break;
    }

    ui->statusComboBox->setCurrentIndex(index);
    ui->statusComboBox->setEnabled(true);

    if (pkmn->compat_has_ability()) {
        const std::array<const char *, 3> abilities { pkmn->abilities() };

        bool has_ability = false;
        for (auto ability : abilities) {
            if (strcmp(ability, "_") == 0)
                continue;

            has_ability = true;
            ui->abilityComboBox->addItem(ability);
        }

        if (has_ability) {
            ui->abilityComboBox->setCurrentIndex(pkmn->ability_id());
            ui->abilityComboBox->setEnabled(allow->set_ability | opt.allow_illegal_modifications);
            ui->abilityComboBox->setEditable(allow->set_ability | opt.allow_illegal_modifications);
        }
    } else {
        ui->abilityComboBox->setEnabled(false);
    }

    if (pkmn->compat_has_held_item()) {
        const item *held_item = pkmn->held_item();
        if (held_item != nullptr) {
            std::span item_db { save.trainer->get_all_items() };
            for (const auto &item : item_db)
                ui->heldItemComboBox->addItem(item.name);
            if (pkmn->has_item())
                ui->heldItemComboBox->setCurrentIndex(
                    save.trainer->item_idx_from_name(pkmn->held_item()->name()));
            else
                ui->heldItemComboBox->setCurrentIndex(0);
            ui->heldItemComboBox->setEnabled(true);
        }
    } else {
        ui->heldItemComboBox->setEnabled(false);
    }

    if (pkmn->compat_has_pokerus()) {
        ui->infectedCheckBox->setChecked(pkmn->is_infected());
        ui->infectedCheckBox->setEnabled(true);
        ui->curedCheckBox->setChecked(pkmn->is_cured());
        ui->curedCheckBox->setEnabled(true);
    } else {
        ui->infectedCheckBox->setEnabled(false);
        ui->curedCheckBox->setEnabled(false);
    }

    if (pkmn->compat_has_shiny()) {
        ui->shinyCheckBox->setEnabled(allow->set_shiny | opt.allow_illegal_modifications);
        ui->shinyCheckBox->setChecked(pkmn->is_shiny());
    } else {
        ui->shinyCheckBox->setEnabled(false);
    }

    if (pkmn->compat_has_egg()) {
        ui->eggCheckBox->setEnabled(allow->set_egg | opt.allow_illegal_modifications);
        ui->eggCheckBox->setChecked(pkmn->is_egg());
    } else {
        ui->eggCheckBox->setEnabled(false);
    }

    if (pkmn->compat_has_origin()) {
        std::vector origin_games { pkmn->origin_games() };
        for (auto &game : origin_games)
            ui->originGameComboBox->addItem(game);
        ui->originGameComboBox->setCurrentIndex(pkmn->game_of_origin());
        ui->originGameComboBox->setEnabled(allow->set_origin_game |
                                           opt.allow_illegal_modifications);
        ui->originGameComboBox->setEditable(allow->set_origin_game |
                                            opt.allow_illegal_modifications);
    } else {
        ui->originGameComboBox->setEnabled(false);
    }

    if (pkmn->compat_has_location_met()) {
        std::span met_locations { pkmn->met_locations_list() };
        const u16 met = pkmn->met_location();
        for (usize i = 0; i < met_locations.size(); ++i) {
            ui->locationComboBox->addItem(met_locations[i].name);
            if (met_locations[i].id == met)
                ui->locationComboBox->setCurrentIndex(i);
        }
        ui->locationComboBox->setEnabled(allow->set_met_location | opt.allow_illegal_modifications);
        ui->locationComboBox->setEditable(allow->set_met_location |
                                          opt.allow_illegal_modifications);
    } else {
        ui->locationComboBox->setEnabled(false);
    }

    if (pkmn->compat_has_pokeball()) {
        std::span pokeballs { pkmn->pokeball_list() };
        for (auto &ball : pokeballs)
            ui->pokeballComboBox->addItem(ball);
        ui->pokeballComboBox->setCurrentIndex(pkmn->pokeball());
        ui->pokeballComboBox->setEnabled(true);
    } else {
        ui->pokeballComboBox->setEnabled(false);
    }

    if (pkmn->compat_has_level_met()) {
        ui->levelMetSpinBox->setValue(pkmn->level_met());
        ui->levelMetSpinBox->setEnabled(allow->set_level_met | opt.allow_illegal_modifications);
        ui->levelMetSpinBox->setReadOnly(!(allow->set_level_met | opt.allow_illegal_modifications));
    } else {
        ui->levelMetSpinBox->setEnabled(false);
    }

    if (pkmn->compat_has_fateful_encounter()) {
        ui->fatefulEncounterCheckBox->setChecked(pkmn->fateful_encounter());
        ui->fatefulEncounterCheckBox->setEnabled(allow->set_fateful_encounter |
                                                 opt.allow_illegal_modifications);
        ui->fatefulEncounterCheckBox->setCheckable(allow->set_fateful_encounter |
                                                   opt.allow_illegal_modifications);
    } else {
        ui->fatefulEncounterCheckBox->setEnabled(false);
    }

    const char *label_text = pkmn->generation() <= 2 ? "DV:" : "IV:";
    const u8 iv_max = pkmn->iv_maximum_value();
    const u16 ev_max = pkmn->ev_maximum_value();

    ui->hpIvSpinBox->setMaximum(iv_max);
    ui->hpIvLabel->setText(label_text);
    ui->atkIvSpinBox->setMaximum(iv_max);
    ui->atkIvLabel->setText(label_text);
    ui->defIvSpinBox->setMaximum(iv_max);
    ui->defIvLabel->setText(label_text);
    ui->speIvSpinBox->setMaximum(iv_max);
    ui->speIvLabel->setText(label_text);

    ui->hpevSpinBox->setMaximum(ev_max);
    ui->atkEvSpinBox->setMaximum(ev_max);
    ui->defEvSpinBox->setMaximum(ev_max);
    ui->spAtkEvSpinBox->setMaximum(ev_max);
    ui->spDefEvSpinBox->setMaximum(ev_max);
    ui->speEvSpinBox->setMaximum(ev_max);

    const bool iv_modifiable = allow->set_ivs | opt.allow_illegal_modifications;

    if (!pkmn->compat_has_spc_eviv()) {
        ui->spAtkIvSpinBox->setEnabled(true);
        ui->spAtkIvSpinBox->setReadOnly(!iv_modifiable);
        ui->spDefIvSpinBox->setEnabled(true);
        ui->spDefIvSpinBox->setReadOnly(!iv_modifiable);
        ui->spAtkEvSpinBox->setEnabled(true);
        ui->spDefEvSpinBox->setEnabled(true);
        ui->spcEvSpinBox->setEnabled(false);
        ui->spDvSpinBox->setEnabled(false);
        ui->spcEvSpinBox->setEnabled(false);
    } else {
        ui->spAtkIvSpinBox->setEnabled(false);
        ui->spDefIvSpinBox->setEnabled(false);
        ui->spAtkEvSpinBox->setEnabled(false);
        ui->spDefEvSpinBox->setEnabled(false);
        ui->spDvSpinBox->setEnabled(true);
        ui->spDvSpinBox->setReadOnly(!iv_modifiable);
        ui->spcEvSpinBox->setEnabled(true);
        ui->spSpinBox->setEnabled(pkmn->compat_has_spc());
    }

    ui->hpIvSpinBox->setEnabled(true);
    ui->atkIvSpinBox->setEnabled(true);
    ui->defIvSpinBox->setEnabled(true);
    ui->speIvSpinBox->setEnabled(true);
    ui->hpIvSpinBox->setReadOnly(!iv_modifiable);
    ui->atkIvSpinBox->setReadOnly(!iv_modifiable);
    ui->defIvSpinBox->setReadOnly(!iv_modifiable);
    ui->speIvSpinBox->setReadOnly(!iv_modifiable);

    ui->hpevSpinBox->setEnabled(true);
    ui->atkEvSpinBox->setEnabled(true);
    ui->defEvSpinBox->setEnabled(true);
    ui->speEvSpinBox->setEnabled(true);

    update_stats_on_ui(pkmn);

    std::span move_list { pkmn->move_list() };
    for (const auto &move : move_list) {
        ui->m1ComboBox->addItem(move.name);
        ui->m2ComboBox->addItem(move.name);
        ui->m3ComboBox->addItem(move.name);
        ui->m4ComboBox->addItem(move.name);
    }

    const bool m_modifiable = allow->set_moveset | opt.allow_illegal_modifications;
    ui->m1ComboBox->setEnabled(m_modifiable);
    ui->m2ComboBox->setEnabled(m_modifiable);
    ui->m3ComboBox->setEnabled(m_modifiable);
    ui->m4ComboBox->setEnabled(m_modifiable);
    ui->m1ComboBox->setCurrentIndex(pkmn->move1());
    ui->m2ComboBox->setCurrentIndex(pkmn->move2());
    ui->m3ComboBox->setCurrentIndex(pkmn->move3());
    ui->m4ComboBox->setCurrentIndex(pkmn->move4());
    ui->m1ComboBox->setEditable(m_modifiable);
    ui->m2ComboBox->setEditable(m_modifiable);
    ui->m3ComboBox->setEditable(m_modifiable);
    ui->m4ComboBox->setEditable(m_modifiable);

    ui->pp1SpinBox->setValue(pkmn->pp1());
    ui->pp2SpinBox->setValue(pkmn->pp2());
    ui->pp3SpinBox->setValue(pkmn->pp3());
    ui->pp4SpinBox->setValue(pkmn->pp4());
    ui->pp1SpinBox->setEnabled(true);
    ui->pp2SpinBox->setEnabled(true);
    ui->pp3SpinBox->setEnabled(true);
    ui->pp4SpinBox->setEnabled(true);

    ui->pp1BonusSpinBox->setValue(pkmn->move1_pp_bonus());
    ui->pp2BonusSpinBox->setValue(pkmn->move2_pp_bonus());
    ui->pp3BonusSpinBox->setValue(pkmn->move3_pp_bonus());
    ui->pp4BonusSpinBox->setValue(pkmn->move4_pp_bonus());
    ui->pp1BonusSpinBox->setEnabled(true);
    ui->pp2BonusSpinBox->setEnabled(true);
    ui->pp3BonusSpinBox->setEnabled(true);
    ui->pp4BonusSpinBox->setEnabled(true);

    ui->m1MaxppLineEdit->setText(QString::fromStdString(std::to_string(pkmn->move1_max_pp())));
    ui->m2MaxppLineEdit->setText(QString::fromStdString(std::to_string(pkmn->move2_max_pp())));
    ui->m3MaxppLineEdit->setText(QString::fromStdString(std::to_string(pkmn->move3_max_pp())));
    ui->m4MaxppLineEdit->setText(QString::fromStdString(std::to_string(pkmn->move4_max_pp())));
    ui->pp1SpinBox->setMaximum(pkmn->move1_max_pp());
    ui->pp2SpinBox->setMaximum(pkmn->move2_max_pp());
    ui->pp3SpinBox->setMaximum(pkmn->move3_max_pp());
    ui->pp4SpinBox->setMaximum(pkmn->move4_max_pp());

    ui->otPidLineEdit->setText(std::string { std::to_string(pkmn->ot_public_id()) }.c_str());
    ui->otSidLineEdit->setText(std::string { std::to_string(pkmn->ot_secret_id()) }.c_str());
    ui->otPidLineEdit->setEnabled(allow->set_ot_pid | opt.allow_illegal_modifications);
    ui->otPidLineEdit->setReadOnly(!(allow->set_ot_pid | opt.allow_illegal_modifications));
    ui->otSidLineEdit->setEnabled(allow->set_ot_sid | opt.allow_illegal_modifications);
    ui->otSidLineEdit->setReadOnly(!(allow->set_ot_sid | opt.allow_illegal_modifications));

    if (pkmn->compat_has_ot_name()) {
        ui->otNameLineEdit->setText(QString::fromStdWString(pkmn->ot_name()));
        ui->otNameLineEdit->setEnabled(true);
    } else {
        ui->otNameLineEdit->setEnabled(false);
    }

    if (pkmn->compat_has_ot_gender()) {
        ui->otGenderComboBox->setCurrentIndex(pkmn->ot_is_female());
        ui->otGenderComboBox->setEnabled(true);
    } else {
        ui->otGenderComboBox->setEnabled(false);
    }

    ui->pkmnSimulateTradePushButton->setEnabled(pkmn->has_trade_evolution());

    block_pkmn_editor_signals(false);
    sel_pkmn = pkmn;
}

void MainWindow::add_item_names_to_combo_box(QComboBox *combo_box, item_category category) const
{
    combo_box->clear();
    std::vector item_names { save.trainer->get_item_names(category) };

    for (const auto &item_name : item_names)
        combo_box->addItem(item_name);
}

void MainWindow::update_pid_on_ui(const pokemon *pkmn) const
{
    assert(pkmn != nullptr);
    ui->pIDLineEdit->setText(
        QString::fromStdString(std::format("0x{:X}", pkmn->personality_value())));
}

void MainWindow::block_pkmn_editor_signals(bool block) const noexcept
{
    ui->hpIvSpinBox->blockSignals(block);
    ui->atkIvSpinBox->blockSignals(block);
    ui->defIvSpinBox->blockSignals(block);
    ui->speIvSpinBox->blockSignals(block);
    ui->spAtkIvSpinBox->blockSignals(block);
    ui->spDefIvSpinBox->blockSignals(block);
    ui->spDvSpinBox->blockSignals(block);

    ui->hpevSpinBox->blockSignals(block);
    ui->atkEvSpinBox->blockSignals(block);
    ui->defEvSpinBox->blockSignals(block);
    ui->speEvSpinBox->blockSignals(block);
    ui->spAtkEvSpinBox->blockSignals(block);
    ui->spDefEvSpinBox->blockSignals(block);
    ui->spcEvSpinBox->blockSignals(block);

    ui->speciesComboBox->blockSignals(block);
    ui->levelSpinBox->blockSignals(block);
    ui->levelMetSpinBox->blockSignals(block);
    ui->pp1SpinBox->blockSignals(block);
    ui->pp2SpinBox->blockSignals(block);
    ui->pp3SpinBox->blockSignals(block);
    ui->pp4SpinBox->blockSignals(block);
    ui->pp1BonusSpinBox->blockSignals(block);
    ui->pp2BonusSpinBox->blockSignals(block);
    ui->pp3BonusSpinBox->blockSignals(block);
    ui->pp4BonusSpinBox->blockSignals(block);

    ui->m1ComboBox->blockSignals(block);
    ui->m2ComboBox->blockSignals(block);
    ui->m3ComboBox->blockSignals(block);
    ui->m4ComboBox->blockSignals(block);
    ui->m1MaxppLineEdit->blockSignals(block);
    ui->m2MaxppLineEdit->blockSignals(block);
    ui->m3MaxppLineEdit->blockSignals(block);
    ui->m4MaxppLineEdit->blockSignals(block);

    ui->otPidLineEdit->blockSignals(block);
    ui->otSidLineEdit->blockSignals(block);
    ui->otNameLineEdit->blockSignals(block);
    ui->otGenderComboBox->blockSignals(block);

    ui->nicknameLineEdit->blockSignals(block);
    ui->pkmnGenderComboBox->blockSignals(block);
    ui->levelSpinBox->blockSignals(block);
    ui->expSpinBox->blockSignals(block);
    ui->natureComboBox->blockSignals(block);
    ui->abilityComboBox->blockSignals(block);
    ui->heldItemComboBox->blockSignals(block);
    ui->originGameComboBox->blockSignals(block);
    ui->locationComboBox->blockSignals(block);
    ui->pokeballComboBox->blockSignals(block);
    ui->statusComboBox->blockSignals(block);
    ui->eggCheckBox->blockSignals(block);
    ui->shinyCheckBox->blockSignals(block);
    ui->infectedCheckBox->blockSignals(block);
    ui->curedCheckBox->blockSignals(block);
    ui->fatefulEncounterCheckBox->blockSignals(block);
}

void MainWindow::block_all_signals(bool block) const noexcept
{
    block_pkmn_editor_signals(block);
    ui->nameLineEdit->blockSignals(block);
    ui->genderComboBox->blockSignals(block);
    ui->moneySpinBox->blockSignals(block);
    ui->coinsSpinBox->blockSignals(block);
    ui->publicIdLineEdit->blockSignals(block);
    ui->secretIdLineEdit->blockSignals(block);
    ui->timePlayedLineEdit->blockSignals(block);

    ui->partyTableWidget->blockSignals(block);
    ui->itemsTableWidget->blockSignals(block);
    ui->ballsTableWidget->blockSignals(block);
    ui->keyItemsTableWidget->blockSignals(block);
    ui->berriesTableWidget->blockSignals(block);
    ui->tmsTableWidget->blockSignals(block);
    ui->pcItemsTableWidget->blockSignals(block);
}

void MainWindow::reset_ui()
{
    block_all_signals(true);
    set_pkmn_in_editor(nullptr);
    reset_table_widget(ui->partyTableWidget);
    reset_table_widget(ui->itemsTableWidget);
    reset_table_widget(ui->ballsTableWidget);
    reset_table_widget(ui->keyItemsTableWidget);
    reset_table_widget(ui->berriesTableWidget);
    reset_table_widget(ui->tmsTableWidget);
    reset_table_widget(ui->pcItemsTableWidget);

    reset_line_edit(ui->nameLineEdit);
    reset_combo_box(ui->genderComboBox);
    reset_spinbox(ui->moneySpinBox);
    reset_spinbox(ui->coinsSpinBox);
    reset_line_edit(ui->publicIdLineEdit);
    reset_line_edit(ui->secretIdLineEdit);
    reset_line_edit(ui->timePlayedLineEdit);

    reset_line_edit(ui->nicknameLineEdit);
    reset_spinbox(ui->levelSpinBox);
    reset_spinbox(ui->expSpinBox);
    reset_spinbox(ui->friendshipSpinBox);
    reset_line_edit(ui->pIDLineEdit);
    reset_spinbox(ui->levelMetSpinBox);
    reset_checkbox(ui->eggCheckBox);
    reset_checkbox(ui->shinyCheckBox);
    reset_checkbox(ui->infectedCheckBox);
    reset_checkbox(ui->curedCheckBox);
    reset_checkbox(ui->fatefulEncounterCheckBox);

    reset_line_edit(ui->otPidLineEdit);
    reset_line_edit(ui->otSidLineEdit);
    reset_line_edit(ui->otNameLineEdit);
    ui->otGenderComboBox->setEnabled(false);

    block_all_signals(false);
}

void MainWindow::reset_checkbox(QCheckBox *checkbox)
{
    checkbox->setChecked(false);
    checkbox->setEnabled(false);
}

void MainWindow::reset_combo_box(QComboBox *combo_box)
{
    combo_box->clear();
    combo_box->setEnabled(false);
}

void MainWindow::reset_line_edit(QLineEdit *line_edit)
{
    line_edit->clear();
    line_edit->setEnabled(false);
}

void MainWindow::reset_spinbox(QSpinBox *spin_box)
{
    spin_box->setValue(0);
    spin_box->setEnabled(false);
}

void MainWindow::reset_table_widget(QTableWidget *table_widget)
{
    table_widget->clearContents();
    table_widget->setRowCount(0);
    table_widget->setEnabled(false);
}

void MainWindow::update_party_table_widget() const
{
    ui->partyTableWidget->blockSignals(true);
    ui->partyTableWidget->clearContents();
    ui->partyTableWidget->setRowCount(save.trainer->pkmn_team().size());
    for (int i = 0; i < save.trainer->pkmn_team().size(); ++i)
        add_pkmn_to_table_widget(ui->partyTableWidget, save.trainer->pkmn_team()[i].get(), i);
    ui->partyTableWidget->blockSignals(false);
}

void MainWindow::set_pkmn_gender_combo_box(const pokemon *pkmn) const
{
    u8 index;
    switch (pkmn->gender()) {
        default:
            index = PKMN_GENDER_COMBOBOX_NA;
            break;
        case pokemon_gender::MALE:
            index = PKMN_GENDER_COMBOBOX_MALE;
            break;
        case pokemon_gender::FEMALE:
            index = PKMN_GENDER_COMBOBOX_FEMALE;
            break;
        case pokemon_gender::GENDERLESS:
            index = PKMN_GENDER_COMBOBOX_GENDERLESS;
            break;
    }

    ui->pkmnGenderComboBox->setCurrentIndex(index);
}

void show_popup_error(const char *err)
{
    QMessageBox msgbox;
    QMessageBox::critical(nullptr, "Error", err);
    msgbox.setFixedSize(500, 200);
    msgbox.show();
    qDebug() << err;
}
