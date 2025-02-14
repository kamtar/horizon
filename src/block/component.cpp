#include "component.hpp"
#include "block.hpp"
#include "logger/logger.hpp"
#include "pool/part.hpp"
#include "nlohmann/json.hpp"
#include "pool/ipool.hpp"

namespace horizon {

json Connection::serialize() const
{
    json j;
    if (net)
        j["net"] = net->uuid;
    else
        j["net"] = nullptr;
    return j;
}

Connection::Connection(const json &j, Block *block)
{
    if (j.at("net").is_null()) {
        net = nullptr;
        return;
    }
    if (block) {
        UUID net_uu = j.at("net").get<std::string>();
        net = block->get_net(net_uu);
        if (net == nullptr) {
            throw std::runtime_error("net " + (std::string)net_uu + " not found");
        }
    }
    else
        net.uuid = j.at("net").get<std::string>();
}

Component::Component(const UUID &uu, const json &j, IPool &pool, Block *block)
    : uuid(uu), entity(pool.get_entity(j.at("entity").get<std::string>())), refdes(j.at("refdes").get<std::string>()),
      value(j.at("value").get<std::string>()), nopopulate(j.value("nopopulate", false))
{
    if (j.count("part")) {
        part = pool.get_part(j.at("part").get<std::string>());
        if (part->entity->uuid != entity->uuid)
            part = nullptr;
    }
    if (j.count("group"))
        group = UUID(j.at("group").get<std::string>());
    if (j.count("tag"))
        tag = UUID(j.at("tag").get<std::string>());

    {
        const json &o = j["connections"];
        for (auto it = o.cbegin(); it != o.cend(); ++it) {
            UUIDPath<2> u(it.key());
            if (entity->gates.count(u.at(0)) > 0) {
                const auto &g = entity->gates.at(u.at(0));
                if (g.unit->pins.count(u.at(1)) > 0) {
                    try {
                        connections.emplace(std::make_pair(u, Connection(it.value(), block)));
                    }
                    catch (const std::exception &e) {
                        Logger::log_critical("error loading connection to " + refdes + "." + g.name + "."
                                                     + g.unit->pins.at(u.at(1)).primary_name,
                                             Logger::Domain::BLOCK, e.what());
                    }
                }
                else {
                    Logger::log_critical("connection to nonexistent pin at " + refdes + "." + g.name,
                                         Logger::Domain::BLOCK);
                }
            }
            else {
                Logger::log_critical("connection to nonexistent gate at " + refdes, Logger::Domain::BLOCK);
            }
        }
    }
    {
        const json &o = j["pin_names"];
        for (const auto &[key, val] : o.items()) {
            UUIDPath<2> u(key);
            if (entity->gates.count(u.at(0)) > 0) {
                const auto &g = entity->gates.at(u.at(0));
                if (g.unit->pins.count(u.at(1)) > 0) {
                    const auto &p = g.unit->pins.at(u.at(1));
                    std::set<int> s;
                    if (val.is_number_integer()) {
                        const auto index = val.get<int>();
                        if ((int)p.names.size() >= index + 1) {
                            s.insert(index);
                        }
                    }
                    else if (val.is_array()) {
                        s = val.get<decltype(s)>();
                    }
                    pin_names.emplace(std::piecewise_construct, std::forward_as_tuple(u), std::forward_as_tuple(s));
                }
            }
        }
    }
    if (j.count("custom_pin_names")) {
        const json &o = j["custom_pin_names"];
        for (const auto &[key, val] : o.items()) {
            UUIDPath<2> u(key);
            if (entity->gates.count(u.at(0)) > 0) {
                const auto &g = entity->gates.at(u.at(0));
                if (g.unit->pins.count(u.at(1)) > 0) {
                    custom_pin_names.emplace(u, val.get<std::string>());
                }
            }
        }
    }
    if (j.count("href")) {
        href = uuid_vec_from_string(j.at("href").get<std::string>());
    }
}
Component::Component(const UUID &uu) : uuid(uu)
{
}
UUID Component::get_uuid() const
{
    return uuid;
}

std::string Component::replace_text(const std::string &t, bool *replaced) const
{
    if (replaced)
        *replaced = false;
    if (t == "$REFDES" || t == "$RD") {
        if (replaced)
            *replaced = true;
        return refdes;
    }
    else if (t == "$VALUE") {
        if (replaced)
            *replaced = true;
        if (part)
            return part->get_value();
        else
            return value;
    }
    else if (t == "$MPN") {
        if (part) {
            if (replaced)
                *replaced = true;
            return part->get_MPN();
        }
        return t;
    }
    else {
        return t;
    }
}

const std::string &Component::get_prefix() const
{
    if (part)
        return part->get_prefix();
    else
        return entity->prefix;
}

json Component::serialize() const
{
    json j;
    j["refdes"] = refdes;
    j["value"] = value;
    j["entity"] = (std::string)entity->uuid;
    j["group"] = (std::string)group;
    j["tag"] = (std::string)tag;
    j["connections"] = json::object();
    if (nopopulate) {
        j["nopopulate"] = true;
    }
    if (part != nullptr) {
        j["part"] = part->uuid;
    }
    for (const auto &it : connections) {
        j["connections"][(std::string)it.first] = it.second.serialize();
    }
    j["pin_names"] = json::object();
    for (const auto &it : pin_names) {
        j["pin_names"][(std::string)it.first] = it.second;
    }
    j["custom_pin_names"] = json::object();
    for (const auto &it : custom_pin_names) {
        if (it.second.size()) {
            j["custom_pin_names"][(std::string)it.first] = it.second;
        }
    }
    if (href.size())
        j["href"] = uuid_vec_to_string(href);
    return j;
}
} // namespace horizon
