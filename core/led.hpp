/*
 * gpio.hpp - LED RaceCam class.
 * 
 * RaceCam Is an app for multiple camera video capture both locally and streaming.
 * Copyright (C) <2026> <William Keeling>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

class LED {
public:
    
LED() {
    int i = {1};
    for (auto& led : ledstreams_) {
	std::string fpath = "/sys/class/leds/LED" + std::to_string(i) + "/brightness";
	led.open("/sys/class/leds/LED" + std::to_string(i) + "/brightness", std::ios::in | std::ios::out);
	if (!led.is_open()) std::cerr << "Failed to open led: "  << fpath;
    }
 }

~LED() {
    for (auto& led : ledstreams_) {
	led.close();
    }
}

std::optional<bool> get(int led) {
    if (!ledstreams_.at(led).is_open()) return {};
    ledstreams_.at(led).seekg(0, std::ios::beg);
    std::string bstr((std::istreambuf_iterator<char>(ledstreams_.at(led))), std::istreambuf_iterator<char>());
    try {
	return std::stoi(bstr);
    }
    catch (const std::exception& e) {
        std::cerr << "led.get stoi error: " << e.what() << std::endl;
	return {};
    }
}

void set(int led, bool val) {
    if (!ledstreams_.at(led).is_open()) return;
    std::string bstr = val ? "1" : "0";
    ledstreams_.at(led) << bstr << std::flush;
}

void toggle(int led) {
    std::optional<bool> b = get(led);
    if (!(b.has_value())) return;
    set(led, !b.value());
    
}

private:
    std::array<std::fstream, 2> ledstreams_;
};
