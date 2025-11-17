#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <thread>
#include <format>
#include <mutex>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <enet/enet.h>
#include <dpp/dpp.h>
#include "Player.h"
#include "Packet.h"
#include "World.h"
#include "proton/variant.hpp"
#include "proton/vector.hpp"

inline dpp::cluster* m_bot = nullptr;

// =======================================================
// Struktur untuk roles - GUNAKAN unordered_map
// =======================================================
//namespace roles {
//    inline std::unordered_map<int, std::string> displayName = {
//        {0, "Player"},
//        {1, "VIP"},
//        {10, "Moderator"},
//        {20, "Developer"},
//        {30, "Super Developer"},
//        {40, "Staff"},
//        {50, "Owner"}
//    };
//}

// =======================================================
// Fungsi helper untuk update role player
// =======================================================
inline void UpdatePlayerRole(const std::string& playerName, int adminLevel, const std::string& updatedBy) {
    // Cari player online
    ENetPeer* target = nullptr;
    player::algorithm::loop_players([&](ENetPeer* peer) {
        if (to_lower(pInfo(peer)->tankIDName) == to_lower(playerName)) {
            target = peer;
        }
        });

    if (target) {
        // Update role untuk player online
        pInfo(target)->adminLevel = adminLevel;

        // Reset semua role
        pInfo(target)->role.vip = false;
        pInfo(target)->role.mod = false;
        pInfo(target)->role.dev = false;
        pInfo(target)->role.superdev = false;
        pInfo(target)->role.staff = false;
        pInfo(target)->role.owner = false;

        // Set role berdasarkan admin level
        if (adminLevel >= 50) {
            pInfo(target)->role.owner = true;
        }
        if (adminLevel >= 40) {
            pInfo(target)->role.staff = true;
        }
        if (adminLevel >= 30) {
            pInfo(target)->role.superdev = true;
        }
        if (adminLevel >= 20) {
            pInfo(target)->role.dev = true;
        }
        if (adminLevel >= 10) {
            pInfo(target)->role.mod = true;
        }
        if (adminLevel >= 1) {
            pInfo(target)->role.vip = true;
        }

        // Update nama dan tampilan
        pInfo(target)->name_color = configure::colorsName3(target);
        nick_update(target, NULL);
        nick_update_2(target, NULL);

        // Kirim notifikasi ke player
        std::string roleName = "Player";
        auto it = roles::displayName.find(adminLevel);
        if (it != roles::displayName.end()) {
            roleName = it->second;
        }
        else {
            roleName = "Level " + std::to_string(adminLevel);
        }

        variants::OnConsoleMessage(target,
            "`o>> Your role has been updated to `5" + roleName + " `oby `0" + updatedBy + "`o!");
        packet_(target, "action|play_sfx\nfile|audio/piano_nice.wav\ndelayMS|0");

        // Save perubahan
        save_player(pInfo(target), false);

        std::cout << "[GIVEROLE] Updated online player " << pInfo(target)->tankIDName
            << " to role " << roleName << " (Level: " << adminLevel << ")" << std::endl;
    }
    else {
        // Update untuk player offline
        std::string playerFile = "database/players/" + to_lower(playerName) + "_.json";
        if (std::filesystem::exists(playerFile)) {
            try {
                std::ifstream ifs(playerFile);
                nlohmann::json j = nlohmann::json::parse(ifs);
                ifs.close();

                std::string originalName = j.contains("name") ? j["name"].get<std::string>() : playerName;

                j["adminlevel"] = adminLevel;
                j["s_adminlevel"] = (adminLevel >= 10 ? 0 : adminLevel);

                // Hapus playmods tertentu jika needed
                if (adminLevel >= 10) {
                    auto playMods = j["playmods"].get<std::vector<nlohmann::json>>();
                    for (auto it = playMods.begin(); it != playMods.end();) {
                        if (it->contains("id") && it->at("id").get<uint16_t>() == 125) {
                            it = playMods.erase(it);
                        }
                        else {
                            ++it;
                        }
                    }
                    j["playmods"] = playMods;
                }

                std::ofstream ofs(playerFile);
                ofs << j.dump(4) << std::endl;
                ofs.close();

                std::cout << "[GIVEROLE] Updated offline player " << originalName
                    << " to level " << adminLevel << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "[GIVEROLE] Error updating offline player: " << e.what() << std::endl;
            }
        }
    }
}

// =======================================================
// Fungsi untuk Discord Broadcast
// =======================================================

inline void DiscordSendModBroadcast(const std::string& role, const std::string& mod_name, const std::string& message) {
    if (mod_name.empty() || message.empty()) return;

    std::vector<ENetPeer*> players;
    player::algorithm::loop_players([&](ENetPeer* peer) {
        if (!peer || pInfo(peer)->radio) return;
        players.push_back(peer);
        });

    std::string worldName = "`bKINGPS";
    std::string tag = "[" + role + "-SB]";
    std::string broadcast_message =
        "CP:_PL:0_OID:_CT:[SB]_ `5" + tag + " from (" + mod_name + ") in [" + worldName + "]`5 : " + message;

    for (ENetPeer* peer : players) {
        if (!peer) continue;
        proton::Variant var("OnConsoleMessage");
        var.push(broadcast_message);
        enet_peer_send(peer, 0, var.pack());
        packet_(peer, "action|play_sfx\nfile|audio/friend_logon.wav\ndelayMS|0");
    }

    std::cout << "[DISCORD BROADCAST] " << role << " | " << mod_name << ": " << message << std::endl;
}

// =======================================================
// Fungsi execute scheduled giveaway
// =======================================================
inline void ExecuteScheduledGiveaway(const GiveawaySchedule& schedule) {
    // Dapatkan nama item dari items vector
    std::string itemName = "Unknown Item";
    if (schedule.itemId >= 0 && schedule.itemId < items.size()) {
        itemName = items[schedule.itemId].name;
    }

    std::string message = "`2** Scheduled Giveaway`o: `0" +
        std::to_string(schedule.quantity) + " `$" +
        itemName + "`o!";
    int itemId = schedule.itemId;
    int quantity = schedule.quantity;

    ENetPacket* packet = proton::Variant{ "OnConsoleMessage" }.push(message).pack();
    ENetPacket* packet2 = proton::Variant{ "OnAddNotification" }.push("interface/large/special_event.rttex", message, "audio/cumbia_horns.wav", 0).pack();

    player::algorithm::loop_players([&](ENetPeer* currentPeer) {
        modify_inventory(currentPeer, itemId, quantity);
        enet_peer_send(currentPeer, 0, packet);
        enet_peer_send(currentPeer, 0, packet2);
        });

    // Broadcast ke Discord via !dsb owner
    std::string discordBroadcast = "🎉 **SCHEDULED GIVEAWAY EXECUTED** 🎉\n" +
        std::string("**Item:** ") + itemName + " (ID: " + std::to_string(schedule.itemId) + ")\n" +
        "**Quantity:** " + std::to_string(schedule.quantity) + "\n" +
        "**Time:** " + (schedule.hour < 10 ? "0" : "") + std::to_string(schedule.hour) + ":" +
        (schedule.minute < 10 ? "0" : "") + std::to_string(schedule.minute) + " WIB";

    // Kirim broadcast ke game via Discord bot
    DiscordSendModBroadcast("`7OWNER", "Giveaway System",
        "Scheduled giveaway executed! " + std::to_string(schedule.quantity) + " x " + itemName);
}

// =======================================================
// Thread untuk check scheduled giveaways
// =======================================================
inline void CheckScheduledGiveaways() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(30)); // Check every 30 seconds

        auto currentTime = getCurrentTimeWIB();

        std::lock_guard<std::mutex> lock(scheduleMutex);

        for (auto& schedule : scheduledGiveaways) {
            if (!schedule.executed &&
                currentTime.tm_hour == schedule.hour &&
                currentTime.tm_min == schedule.minute) {

                // Execute giveaway
                ExecuteScheduledGiveaway(schedule);
                schedule.executed = true;
            }

            // Reset executed flag if time has passed
            if (schedule.executed &&
                (currentTime.tm_hour != schedule.hour || currentTime.tm_min != schedule.minute)) {
                schedule.executed = false;
            }
        }
    }
}

// =======================================================
// Start scheduler thread
// =======================================================
inline void StartGiveawayScheduler() {
    std::thread schedulerThread(CheckScheduledGiveaways);
    schedulerThread.detach();
}

// =======================================================
// Kirim pesan ke Discord Webhook
// =======================================================
inline void DiscordWebhookSend(const std::string& webhook_url, const std::string& content) {
    try {
        dpp::webhook wh(webhook_url);
        dpp::message msg;
        msg.set_content(content);

        if (!m_bot) {
            std::cerr << "[Discord] ❌ m_bot belum diinisialisasi.\n";
            return;
        }
        m_bot->execute_webhook(wh, msg);
        std::cout << "[Discord] ✅ Webhook terkirim.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[Discord] Exception: " << e.what() << std::endl;
    }
}

// =======================================================
// Inisialisasi Bot Discord dengan command giveaway
// =======================================================
inline void InitializeDiscordBot(const std::string& token) {
    try {
        m_bot = new dpp::cluster(token, dpp::i_all_intents);

        m_bot->on_ready([](const dpp::ready_t& event) {
            std::cout << "[DiscordBot] ✅ Online sebagai " << event.from->creator->me.username << std::endl;

            // Daftarkan slash commands
            dpp::slashcommand dsb_cmd("dsb", "Broadcast ke game dengan role", event.from->creator->me.id);
            dsb_cmd.add_option(dpp::command_option(dpp::co_string, "role", "Pilih role", true)
                .add_choice(dpp::command_option_choice("vip", "vip"))
                .add_choice(dpp::command_option_choice("mod", "mod"))
                .add_choice(dpp::command_option_choice("smod", "smod"))
                .add_choice(dpp::command_option_choice("dev", "dev"))
                .add_choice(dpp::command_option_choice("sdev", "sdev"))
                .add_choice(dpp::command_option_choice("staff", "staff"))
                .add_choice(dpp::command_option_choice("owner", "owner"))
            );
            dsb_cmd.add_option(dpp::command_option(dpp::co_string, "text", "Pesan yang ingin disiarkan", true));

            // Command untuk scheduled giveaway
            dpp::slashcommand schedgive_cmd("schedgive", "Jadwalkan giveaway otomatis", event.from->creator->me.id);
            schedgive_cmd.add_option(dpp::command_option(dpp::co_integer, "hour", "Jam WIB (0-23)", true));
            schedgive_cmd.add_option(dpp::command_option(dpp::co_integer, "minute", "Menit WIB (0-59)", true));
            schedgive_cmd.add_option(dpp::command_option(dpp::co_integer, "itemid", "ID Item", true));
            schedgive_cmd.add_option(dpp::command_option(dpp::co_integer, "quantity", "Jumlah item (1-200)", true));

            // Command untuk melihat jadwal
            dpp::slashcommand viewschedule_cmd("viewschedule", "Lihat jadwal giveaway yang aktif", event.from->creator->me.id);

            // Command untuk menghapus jadwal
            dpp::slashcommand deleteschedule_cmd("deleteschedule", "Hapus jadwal giveaway", event.from->creator->me.id);
            deleteschedule_cmd.add_option(dpp::command_option(dpp::co_integer, "index", "Nomor jadwal yang akan dihapus", true));

            // Command untuk give item
            dpp::slashcommand giveitem_cmd("giveitem", "Give item ke player", event.from->creator->me.id);
            giveitem_cmd.add_option(dpp::command_option(dpp::co_string, "player", "Nama player", true));
            giveitem_cmd.add_option(dpp::command_option(dpp::co_integer, "itemid", "ID Item", true));
            giveitem_cmd.add_option(dpp::command_option(dpp::co_integer, "quantity", "Jumlah item", true));

            // Command untuk give role
            dpp::slashcommand giverole_cmd("giverole", "Set role/level untuk player", event.from->creator->me.id);
            giverole_cmd.add_option(dpp::command_option(dpp::co_string, "player", "Nama player", true));
            giverole_cmd.add_option(dpp::command_option(dpp::co_integer, "level", "Level role (0=Player, 1=VIP, 10=Mod, 20=Dev, 30=SDev, 40=Staff, 50=Owner, atau custom)", true));

            // Command untuk boost events
            dpp::slashcommand boostgem_cmd("boostgem", "Aktifkan multiplier gems untuk semua player", event.from->creator->me.id);
            boostgem_cmd.add_option(dpp::command_option(dpp::co_integer, "multiplier", "Multiplier gems (1-10)", true));

            dpp::slashcommand boostxp_cmd("boostxp", "Aktifkan multiplier XP untuk semua player", event.from->creator->me.id);
            boostxp_cmd.add_option(dpp::command_option(dpp::co_integer, "multiplier", "Multiplier XP (1-10)", true));

            dpp::slashcommand boostprovider_cmd("boostprovider", "Aktifkan multiplier provider untuk semua player", event.from->creator->me.id);
            boostprovider_cmd.add_option(dpp::command_option(dpp::co_integer, "multiplier", "Multiplier provider (1-10)", true));

            // Command untuk nuke world
            dpp::slashcommand nuke_cmd("nuke", "Nuke/Unnuke world tertentu", event.from->creator->me.id);
            nuke_cmd.add_option(dpp::command_option(dpp::co_string, "world", "Nama world yang akan di-nuke/un-nuke", true));

            event.from->creator->global_bulk_command_create({
                dsb_cmd, schedgive_cmd, viewschedule_cmd, deleteschedule_cmd, giveitem_cmd, giverole_cmd,
                boostgem_cmd, boostxp_cmd, boostprovider_cmd, nuke_cmd
                });

            // Start giveaway scheduler
            StartGiveawayScheduler();
            });

        // Event: slash command handler
        m_bot->on_slashcommand([](const dpp::slashcommand_t& event) {
            std::string command_name = event.command.get_command_name();

            if (command_name == "dsb") {
                std::string role = std::get<std::string>(event.get_parameter("role"));
                std::string text = std::get<std::string>(event.get_parameter("text"));
                std::string role_upper;

                // Ubah role ke format label (SB Style)
                if (role == "vip") role_upper = "`1VIP";
                else if (role == "mod") role_upper = "`2MOD";
                else if (role == "smod") role_upper = "`3SMOD";
                else if (role == "dev") role_upper = "`4DEV";
                else if (role == "sdev") role_upper = "`5SDEV";
                else if (role == "staff") role_upper = "`6STAFF";
                else if (role == "owner") role_upper = "`7OWNER";
                else {
                    event.reply("❌ Role tidak dikenal. Gunakan salah satu dari: vip, mod, smod, dev, sdev, staff, owner.");
                    return;
                }

                std::string username = event.command.usr.username;
                DiscordSendModBroadcast(role_upper, username, text);

                dpp::embed embed = dpp::embed()
                    .set_color(dpp::colors::green)
                    .set_title("✅ Broadcast Sent")
                    .add_field("Role", role_upper, true)
                    .add_field("User", username, true)
                    .add_field("Message", text)
                    .set_timestamp(time(nullptr));

                event.reply(dpp::message().add_embed(embed));
            }
            else if (command_name == "giveitem") {
                std::string playerName = std::get<std::string>(event.get_parameter("player"));
                int64_t itemId = std::get<int64_t>(event.get_parameter("itemid"));
                int64_t quantity = std::get<int64_t>(event.get_parameter("quantity"));

                // Validasi input
                if (quantity < 1 || quantity > 1000) {
                    event.reply("❌ Quantity harus antara 1-1000!");
                    return;
                }

                if (itemId < 0 || itemId >= (int64_t)items.size()) {
                    event.reply("❌ Item ID tidak valid!");
                    return;
                }

                // Cari target player
                ENetPeer* target = nullptr;
                for (ENetPeer* currentPeer = server->peers; currentPeer < &server->peers[server->peerCount]; ++currentPeer) {
                    if (currentPeer->state != ENET_PEER_STATE_CONNECTED) continue;
                    if (to_lower(pInfo(currentPeer)->tankIDName) == to_lower(playerName)) {
                        target = currentPeer;
                        break;
                    }
                }

                if (!target) {
                    event.reply("❌ Player tidak ditemukan atau sedang offline!");
                    return;
                }

                // Restricted items check
                std::vector<int> restrictedItems = { 274, 276, 8470, 278, 732, 7782, 7784, 5138, 5136, 11550, 14522 };
                if (std::find(restrictedItems.begin(), restrictedItems.end(), itemId) != restrictedItems.end()) {
                    event.reply("❌ Item ini restricted dan tidak bisa diberikan melalui Discord!");
                    return;
                }

                // Dapatkan nama item
                std::string itemName = "Unknown";
                if (itemId >= 0 && itemId < (int64_t)items.size()) {
                    itemName = items[itemId].name;
                }
                int itemids = static_cast<int>(itemId);
                int quantiti = static_cast<int>(quantity);
                // Give item ke player
                modify_inventory(target, itemids, quantiti);

                // Kirim pesan ke game
                std::string discordUsername = event.command.usr.username;
                std::string gameMessage = "`2[DISCORD] `5" + discordUsername + "`2 gave `5" +
                    std::to_string(quantity) + " `2" + itemName + "`2 to `5" + pInfo(target)->tankIDName;

                player::algorithm::loop_players([&](ENetPeer* peer) {
                    if (pInfo(peer)->role.mod || pInfo(peer)->role.owner) {
                        variants::OnConsoleMessage(peer, gameMessage);
                    }
                    });

                // Kirim ke target player
                variants::OnConsoleMessage(target, "`2You received `5" + std::to_string(quantity) + " `2" + itemName + "`2 from Discord user `5" + discordUsername);

                // Reply ke Discord
                dpp::embed embed = dpp::embed()
                    .set_color(dpp::colors::green)
                    .set_title("✅ Item Given Successfully")
                    .add_field("Player", pInfo(target)->tankIDName, true)
                    .add_field("Item", itemName + " (ID: " + std::to_string(itemId) + ")", true)
                    .add_field("Quantity", std::to_string(quantity), true)
                    .add_field("Given by", discordUsername, true)
                    .set_timestamp(time(nullptr));

                event.reply(dpp::message().add_embed(embed));

                // Log ke console
                std::cout << "[DISCORD GIVEITEM] " << discordUsername << " gave " << quantity << " " << itemName
                    << " to " << pInfo(target)->tankIDName << std::endl;
            }
            else if (command_name == "giverole") {
                std::string playerName = std::get<std::string>(event.get_parameter("player"));
                int64_t level = std::get<int64_t>(event.get_parameter("level"));
                std::string username = event.command.usr.username;

                // Validasi level
                if (level < 0 || level > 100) {
                    event.reply("❌ Level harus antara 0-100!");
                    return;
                }

                // Validasi nama player tidak kosong
                if (playerName.empty()) {
                    event.reply("❌ Nama player tidak boleh kosong!");
                    return;
                }

                // Cek apakah player exists (online atau offline)
                bool playerExists = false;
                std::string displayName = playerName;

                // Cek player online
                player::algorithm::loop_players([&](ENetPeer* peer) {
                    if (to_lower(pInfo(peer)->tankIDName) == to_lower(playerName)) {
                        playerExists = true;
                        displayName = pInfo(peer)->tankIDName;
                    }
                    });

                // Cek player offline
                if (!playerExists) {
                    std::string playerFile = "database/players/" + to_lower(playerName) + "_.json";
                    if (std::filesystem::exists(playerFile)) {
                        playerExists = true;
                        try {
                            std::ifstream ifs(playerFile);
                            nlohmann::json j = nlohmann::json::parse(ifs);
                            ifs.close();
                            if (j.contains("name")) {
                                displayName = j["name"].get<std::string>();
                            }
                        }
                        catch (const std::exception& e) {
                            std::cerr << "[GIVEROLE] Error reading player file: " << e.what() << std::endl;
                        }
                    }
                }

                if (!playerExists) {
                    event.reply("❌ Player tidak ditemukan!");
                    return;
                }

                // Update role player
                UpdatePlayerRole(playerName, static_cast<int>(level), username + " (Discord)");

                // Dapatkan nama role
                std::string roleName = "Custom Role";
                auto it = roles::displayName.find(static_cast<int>(level));
                if (it != roles::displayName.end()) {
                    roleName = it->second;
                }
                else {
                    roleName = "Level " + std::to_string(level);
                }

                // Buat embed response
                dpp::embed embed = dpp::embed()
                    .set_color(level == 0 ? dpp::colors::white :
                        level == 1 ? dpp::colors::light_gray :
                        level == 10 ? dpp::colors::green :
                        level == 20 ? dpp::colors::blue :
                        level == 30 ? dpp::colors::dark_blue :
                        level == 40 ? dpp::colors::yellow :
                        level == 50 ? dpp::colors::red : dpp::colors::purple)
                    .set_title("🎭 Role Updated")
                    .add_field("Player", displayName, true)
                    .add_field("Level", std::to_string(level), true)
                    .add_field("Role", roleName, true)
                    .add_field("Updated by", username, true)
                    .set_footer(dpp::embed_footer().set_text("Role berhasil diupdate"))
                    .set_timestamp(time(nullptr));

                event.reply(dpp::message().add_embed(embed));

                // Broadcast ke mods di game
                std::string gameBroadcast = "`2[DISCORD] `5" + username + "`2 set role `5" + displayName +
                    "`2 to `5" + roleName + "`2 (Level: " + std::to_string(level) + ")";

                player::algorithm::loop_players([&](ENetPeer* peer) {
                    if (pInfo(peer)->role.mod || pInfo(peer)->role.dev || pInfo(peer)->role.owner) {
                        variants::OnConsoleMessage(peer, gameBroadcast);
                    }
                    });

                // Log ke console
                std::cout << "[DISCORD GIVEROLE] " << username << " set " << displayName
                    << " to role " << roleName << " (Level: " << level << ")" << std::endl;
            }
            else if (command_name == "schedgive") {
                int64_t hour = std::get<int64_t>(event.get_parameter("hour"));
                int64_t minute = std::get<int64_t>(event.get_parameter("minute"));
                int64_t itemId = std::get<int64_t>(event.get_parameter("itemid"));
                int64_t quantity = std::get<int64_t>(event.get_parameter("quantity"));

                // Validasi input
                if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
                    event.reply("❌ Waktu tidak valid! Hour (0-23), Minute (0-59)");
                    return;
                }

                if (quantity < 1 || quantity > 200) {
                    event.reply("❌ Quantity harus antara 1-200!");
                    return;
                }

                if (itemId < 2 || itemId >= (int64_t)items.size()) {
                    event.reply("❌ Item ID tidak valid!");
                    return;
                }

                // Dapatkan nama item
                std::string itemName = "Unknown";
                if (itemId >= 0 && itemId < (int64_t)items.size()) {
                    itemName = items[itemId].name;
                }

                // Tambahkan ke jadwal
                std::lock_guard<std::mutex> lock(scheduleMutex);
                std::string username = event.command.usr.username;

                GiveawaySchedule newSchedule;
                newSchedule.hour = static_cast<int>(hour);
                newSchedule.minute = static_cast<int>(minute);
                newSchedule.itemId = static_cast<int>(itemId);
                newSchedule.quantity = static_cast<int>(quantity);
                newSchedule.executed = false;
                newSchedule.scheduledBy = username;

                scheduledGiveaways.push_back(newSchedule);

                auto currentTime = getCurrentTimeWIB();
                std::string timeStr = (hour < 10 ? "0" : "") + std::to_string(hour) + ":" +
                    (minute < 10 ? "0" : "") + std::to_string(minute) + " WIB";
                std::string currentTimeStr = (currentTime.tm_hour < 10 ? "0" : "") + std::to_string(currentTime.tm_hour) + ":" +
                    (currentTime.tm_min < 10 ? "0" : "") + std::to_string(currentTime.tm_min) + " WIB";

                dpp::embed embed = dpp::embed()
                    .set_color(dpp::colors::blue)
                    .set_title("✅ Giveaway Scheduled")
                    .add_field("Waktu", timeStr, true)
                    .add_field("Item", itemName + " (ID: " + std::to_string(itemId) + ")", true)
                    .add_field("Quantity", std::to_string(quantity), true)
                    .add_field("Scheduled by", username, true)
                    .add_field("Waktu Sekarang", currentTimeStr, true)
                    .set_footer(dpp::embed_footer().set_text("Giveaway akan dieksekusi otomatis"))
                    .set_timestamp(time(nullptr));

                event.reply(dpp::message().add_embed(embed));

                // Broadcast info ke game
                DiscordSendModBroadcast("`7OWNER", "Giveaway System",
                    "New scheduled giveaway! " + std::to_string(quantity) + " x " + itemName + " at " + timeStr + " (by: " + username + ")");
            }
            else if (command_name == "viewschedule") {
                std::lock_guard<std::mutex> lock(scheduleMutex);

                if (scheduledGiveaways.empty()) {
                    event.reply("❌ Tidak ada jadwal giveaway yang aktif.");
                    return;
                }

                dpp::embed embed = dpp::embed()
                    .set_color(dpp::colors::yellow)
                    .set_title("📅 Scheduled Giveaways")
                    .set_description("Berikut adalah jadwal giveaway yang aktif:");

                for (size_t i = 0; i < scheduledGiveaways.size(); i++) {
                    const auto& schedule = scheduledGiveaways[i];
                    std::string status = schedule.executed ? "✅ Executed" : "⏳ Pending";

                    // Dapatkan nama item
                    std::string itemName = "Unknown";
                    if (schedule.itemId >= 0 && schedule.itemId < items.size()) {
                        itemName = items[schedule.itemId].name;
                    }

                    std::string fieldValue =
                        "**Item:** " + itemName + " (ID: " + std::to_string(schedule.itemId) + ")\n" +
                        "**Quantity:** " + std::to_string(schedule.quantity) + "\n" +
                        "**Status:** " + status + "\n" +
                        "**By:** " + schedule.scheduledBy;

                    std::string timeLabel = std::to_string(i + 1) + " - " +
                        (schedule.hour < 10 ? "0" : "") + std::to_string(schedule.hour) + ":" +
                        (schedule.minute < 10 ? "0" : "") + std::to_string(schedule.minute) + " WIB";

                    embed.add_field(timeLabel, fieldValue, true);
                }

                event.reply(dpp::message().add_embed(embed));
            }

            else if (command_name == "deleteschedule") {
                int64_t index = std::get<int64_t>(event.get_parameter("index"));

                std::lock_guard<std::mutex> lock(scheduleMutex);

                if (index < 1 || index >(int64_t)scheduledGiveaways.size()) {
                    event.reply("❌ Index tidak valid! Gunakan /viewschedule untuk melihat daftar.");
                    return;
                }

                size_t idx = index - 1;
                auto removed = scheduledGiveaways[idx];
                scheduledGiveaways.erase(scheduledGiveaways.begin() + idx);

                // Dapatkan nama item
                std::string itemName = "Unknown";
                if (removed.itemId >= 0 && removed.itemId < items.size()) {
                    itemName = items[removed.itemId].name;
                }

                dpp::embed embed = dpp::embed()
                    .set_color(dpp::colors::red)
                    .set_title("🗑️ Schedule Deleted")
                    .add_field("Waktu",
                        (removed.hour < 10 ? "0" : "") + std::to_string(removed.hour) + ":" +
                        (removed.minute < 10 ? "0" : "") + std::to_string(removed.minute) + " WIB", true)
                    .add_field("Item", itemName + " (ID: " + std::to_string(removed.itemId) + ")", true)
                    .add_field("Quantity", std::to_string(removed.quantity), true)
                    .add_field("Deleted by", event.command.usr.username, true)
                    .set_timestamp(time(nullptr));

                event.reply(dpp::message().add_embed(embed));

                // Broadcast info penghapusan
                DiscordSendModBroadcast("`7OWNER", "Giveaway System",
                    "Scheduled giveaway deleted: " + std::to_string(removed.quantity) + " x " + itemName + " at " +
                    (removed.hour < 10 ? "0" : "") + std::to_string(removed.hour) + ":" +
                    (removed.minute < 10 ? "0" : "") + std::to_string(removed.minute) + " WIB");
            }
            // =======================================================
            // Command Boost Events (Gems, XP, Provider)
            // =======================================================
            else if (command_name == "boostgem") {
                int64_t multiplier = std::get<int64_t>(event.get_parameter("multiplier"));

                // Validasi multiplier
                if (multiplier < 1 || multiplier > 10) {
                    event.reply("❌ Multiplier harus antara 1-10!");
                    return;
                }

                events::gems = static_cast<int>(multiplier);

                std::string username = event.command.usr.username;
                std::string gameMessage = std::format("`4SYSTEM: `w{} `ohas runned the `5x{} `ogems multiplier events. Breaks your farmables now!", username, std::to_string(events::gems));
                std::string notificationMessage = std::format("`wThere is `5{}x `wmultiplier gems events, hosted by `5{}`w!", std::to_string(events::gems), username);

                ENetPacket* packet = proton::Variant{ "OnConsoleMessage" }.push(gameMessage).pack();
                ENetPacket* packet2 = proton::Variant{ "OnAddNotification" }.push("interface/large/special_event.rttex", notificationMessage, "audio/cumbia_horns.wav", 0).pack();

                player::algorithm::loop_players([&](ENetPeer* currentPeer) {
                    enet_peer_send(currentPeer, 0, packet);
                    enet_peer_send(currentPeer, 0, packet2);
                    });

                dpp::embed embed = dpp::embed()
                    .set_color(dpp::colors::purple)
                    .set_title("💎 Gems Boost Activated")
                    .add_field("Multiplier", "x" + std::to_string(multiplier), true)
                    .add_field("Activated by", username, true)
                    .add_field("Status", "✅ Active", true)
                    .set_footer(dpp::embed_footer().set_text("All players will receive multiplied gems"))
                    .set_timestamp(time(nullptr));

                event.reply(dpp::message().add_embed(embed));

                // Log ke console
                std::cout << "[DISCORD BOOST] " << username << " activated x" << multiplier << " gems boost" << std::endl;
            }
            else if (command_name == "boostxp") {
                int64_t multiplier = std::get<int64_t>(event.get_parameter("multiplier"));

                // Validasi multiplier
                if (multiplier < 1 || multiplier > 10) {
                    event.reply("❌ Multiplier harus antara 1-10!");
                    return;
                }

                events::xp = static_cast<int>(multiplier);

                std::string username = event.command.usr.username;
                std::string gameMessage = std::format("`4SYSTEM: `w{} `ohas runned the `5x{} `oexp multiplier events. Breaks your farmables now!", username, std::to_string(events::xp));
                std::string notificationMessage = std::format("`wThere is `5{}x `wmultiplier exp events, hosted by `5{}`w!", std::to_string(events::xp), username);

                ENetPacket* packet = proton::Variant{ "OnConsoleMessage" }.push(gameMessage).pack();
                ENetPacket* packet2 = proton::Variant{ "OnAddNotification" }.push("interface/large/special_event.rttex", notificationMessage, "audio/cumbia_horns.wav", 0).pack();

                player::algorithm::loop_players([&](ENetPeer* currentPeer) {
                    enet_peer_send(currentPeer, 0, packet);
                    enet_peer_send(currentPeer, 0, packet2);
                    });

                dpp::embed embed = dpp::embed()
                    .set_color(dpp::colors::blue)
                    .set_title("⭐ XP Boost Activated")
                    .add_field("Multiplier", "x" + std::to_string(multiplier), true)
                    .add_field("Activated by", username, true)
                    .add_field("Status", "✅ Active", true)
                    .set_footer(dpp::embed_footer().set_text("All players will receive multiplied XP"))
                    .set_timestamp(time(nullptr));

                event.reply(dpp::message().add_embed(embed));

                // Log ke console
                std::cout << "[DISCORD BOOST] " << username << " activated x" << multiplier << " XP boost" << std::endl;
            }
            else if (command_name == "boostprovider") {
                int64_t multiplier = std::get<int64_t>(event.get_parameter("multiplier"));

                // Validasi multiplier
                if (multiplier < 1 || multiplier > 10) {
                    event.reply("❌ Multiplier harus antara 1-10!");
                    return;
                }

                events::provider = static_cast<int>(multiplier);

                std::string username = event.command.usr.username;
                std::string gameMessage = std::format("`4SYSTEM: `w{} `ohas runned the `5x{} `oprovider multiplier events.", username, std::to_string(events::provider));
                std::string notificationMessage = std::format("`wThere is `5{}x `wmultiplier provider events, hosted by `5{}`w!", std::to_string(events::provider), username);

                ENetPacket* packet = proton::Variant{ "OnConsoleMessage" }.push(gameMessage).pack();
                ENetPacket* packet2 = proton::Variant{ "OnAddNotification" }.push("interface/large/special_event.rttex", notificationMessage, "audio/cumbia_horns.wav", 0).pack();

                player::algorithm::loop_players([&](ENetPeer* currentPeer) {
                    enet_peer_send(currentPeer, 0, packet);
                    enet_peer_send(currentPeer, 0, packet2);
                    });

                dpp::embed embed = dpp::embed()
                    .set_color(dpp::colors::green)
                    .set_title("🔧 Provider Boost Activated")
                    .add_field("Multiplier", "x" + std::to_string(multiplier), true)
                    .add_field("Activated by", username, true)
                    .add_field("Status", "✅ Active", true)
                    .set_footer(dpp::embed_footer().set_text("All players will receive multiplied provider drops"))
                    .set_timestamp(time(nullptr));

                event.reply(dpp::message().add_embed(embed));

                // Log ke console
                std::cout << "[DISCORD BOOST] " << username << " activated x" << multiplier << " provider boost" << std::endl;
            }
            else if (command_name == "nuke") {
                std::string worldName = std::get<std::string>(event.get_parameter("world"));
                std::string username = event.command.usr.username;

                // Cek apakah ada player di world tersebut untuk memastikan world exists
                bool worldExists = false;
                player::algorithm::loop_players([&](ENetPeer* peer) {
                    if (pInfo(peer)->world == worldName) {
                        worldExists = true;
                    }
                    });

                if (!worldExists) {
                    event.reply("❌ World tidak ditemukan atau tidak ada player di dalamnya!");
                    return;
                }

                // Untuk versi sederhana, kita langsung nuke tanpa akses struktur World
                std::string action = "NUKED";

                // Kirim sound effect dan pesan
                std::string consoleMsg = "`o>> `4" + worldName + " `4was nuked from orbit by " + username + " (DISCORD)`o. It's the only way to be sure.";

                // Buat packet sound effect
                std::string sfxData = "action|play_sfx\nfile|audio/bigboom.wav\ndelayMS|0";
                ENetPacket* sfxPacket = enet_packet_create(sfxData.c_str(), sfxData.length() + 1, ENET_PACKET_FLAG_RELIABLE);

                // Buat packet console message
                ENetPacket* msgPacket = proton::Variant{ "OnConsoleMessage" }.push(consoleMsg).pack();

                // Kick semua player non-staff dari world yang di-nuke dan kirim notifikasi
                int playersKicked = 0;
                player::algorithm::loop_players([&](ENetPeer* currentPeer) {
                    if (!currentPeer || currentPeer->data == NULL || pInfo(currentPeer)->radio) return;

                    // Kirim sound effect dan pesan ke semua player
                    enet_peer_send(currentPeer, 0, sfxPacket);
                    enet_peer_send(currentPeer, 0, msgPacket);

                    // Kick player non-staff yang berada di world yang di-nuke
                    if (pInfo(currentPeer)->world == worldName &&
                        !pInfo(currentPeer)->role.mod &&
                        !pInfo(currentPeer)->role.dev &&
                        !pInfo(currentPeer)->role.owner) {
                        exit_(currentPeer);
                        playersKicked++;
                    }
                    });

                dpp::embed embed = dpp::embed()
                    .set_color(dpp::colors::red)
                    .set_title("💥 World Nuked")
                    .add_field("World", worldName, true)
                    .add_field("Action", "NUKED", true)
                    .add_field("By", username, true)
                    .add_field("Players Kicked", std::to_string(playersKicked), true)
                    .add_field("Status", "🚫 World locked", true)
                    .set_timestamp(time(nullptr));

                event.reply(dpp::message().add_embed(embed));

                // Broadcast info ke game via dsb owner
                DiscordSendModBroadcast("`7OWNER", "Nuke System",
                    "World `#" + worldName + "` has been NUKED from orbit! " +
                    std::to_string(playersKicked) + " non-staff players kicked. (by: " + username + ")");

                // Log ke console
                std::cout << "[DISCORD NUKE] " << username << " NUKED world: " << worldName << " (" << playersKicked << " players kicked)" << std::endl;
            }
            });

        // Jalankan bot di thread terpisah
        std::thread([] {
            try {
                m_bot->start(true);
                while (true) std::this_thread::sleep_for(std::chrono::seconds(60));
            }
            catch (const std::exception& e) {
                std::cerr << "[DiscordBot] ❌ Error: " << e.what() << std::endl;
            }
            }).detach();

    }
    catch (const std::exception& e) {
        std::cerr << "[DiscordBot] ⚠️ Gagal inisialisasi: " << e.what() << std::endl;
    }
}