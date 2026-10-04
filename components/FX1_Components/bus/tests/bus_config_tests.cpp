#include "bus/config.h"
#include <iostream>
#include <stdexcept>

int main() {
    unsigned checks=0,failures=0;
    auto check=[&](const bus::BusConfig& c,bool valid) {
        bool accepted=true;
        try { c.validate(); } catch(const std::invalid_argument&) { accepted=false; }
        ++checks;
        if (accepted!=valid) { ++failures; std::cerr<<"Config case "<<checks<<" failed\n"; }
    };
    bus::BusConfig c;
    check(c,true);
    c.pp1_ports=6;c.pp0_ports=5;check(c,true);
    c=bus::BusConfig{};c.pp0_ports=0;check(c,false);
    c=bus::BusConfig{};c.peribus1[0].port=c.pp1_ports;check(c,false);
    c=bus::BusConfig{};c.sysbus1[0].end=c.sysbus1[0].begin;check(c,false);
    c=bus::BusConfig{};c.sysbus0.push_back(c.sysbus0.front());check(c,false);
    c=bus::BusConfig{};c.sysbus1[4].translate=true;check(c,false);
    c=bus::BusConfig{};c.sysbus1[3].translate=true;check(c,false);
    c=bus::BusConfig{};c.sysbus0[2].translate=true;check(c,false);
    c=bus::BusConfig{};c.sysbus1[4].end=bus::config::SB0+0x10000;check(c,false);
    c=bus::BusConfig{};c.peribus0[0].begin=0x60000000;c.peribus0[0].end=0x60001000;check(c,false);
    c=bus::BusConfig{};c.sysbus1[2]={"RAM",0x80000000,0x90000000,2,true};check(c,true);
    std::cout<<"Config: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
