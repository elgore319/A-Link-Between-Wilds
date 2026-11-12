#include "socket_api.hpp"

#include "lib.hpp"
#include "lib/util/sys/modules.hpp"
#include "program/loggers.hpp"
#include "program/net/elf_symbols.hpp"

namespace albw::net {

    namespace {
        /* Candidate mangled names, oldest SDK spelling first. */
        constexpr const char* InitializeNames[] = {
            "_ZN2nn6socket10InitializeEPvmmi",
        };
        constexpr const char* SocketNames[] = {
            "_ZN2nn6socket6SocketEiii",
            "_ZN2nn6socket6SocketENS0_6FamilyENS0_4TypeENS0_8ProtocolE",
        };
        constexpr const char* ConnectNames[] = {
            "_ZN2nn6socket7ConnectEiPK8sockaddrj",
            "_ZN2nn6socket7ConnectEiPKNS0_8SockAddrEj",
        };
        constexpr const char* SendNames[] = {
            "_ZN2nn6socket4SendEiPKvmi",
            "_ZN2nn6socket4SendEiPKvmNS0_7MsgFlagE",
        };
        constexpr const char* RecvNames[] = {
            "_ZN2nn6socket4RecvEiPvmi",
            "_ZN2nn6socket4RecvEiPvmNS0_7MsgFlagE",
        };
        constexpr const char* GetLastErrnoNames[] = {
            "_ZN2nn6socket12GetLastErrnoEv",
            "_ZN2nn6socket12GetLastErrorEv",
        };

        /* Calls fn(symbol_table, module_info) for every loaded module except this one. */
        template<typename Fn>
        void ForEachGameModule(Fn&& fn) {
            const uintptr_t self = exl::util::GetSelfModuleInfo().m_Total.m_Start;
            for (int i = static_cast<int>(exl::util::ModuleIndex::Start); i < static_cast<int>(exl::util::ModuleIndex::End); i++) {
                auto idx = static_cast<exl::util::ModuleIndex>(i);
                if (!exl::util::HasModule(idx))
                    continue;
                const auto& info = exl::util::GetModuleInfo(idx);
                if (info.m_Total.m_Start == self || info.m_Mod == nullptr)
                    continue;
                elf::SymbolTable table(info.m_Total.m_Start,
                                       reinterpret_cast<const elf::Dyn*>(info.m_Mod->GetDynamic()));
                if (table.Valid() && fn(table, info))
                    return;
            }
        }

        template<typename T, size_t N>
        bool Lookup(T& out, const char* label, const char* const (&names)[N]) {
            bool found = false;
            ForEachGameModule([&](const elf::SymbolTable& table, const exl::util::ModuleInfo& info) {
                for (const char* name : names) {
                    uintptr_t addr = table.Find(name);
                    if (addr == 0)
                        continue;
                    out = reinterpret_cast<T>(addr);
                    auto mod = info.GetModuleName();
                    Logging.Log("[albw] found %s in %.*s as %s", label,
                                static_cast<int>(mod.size()), mod.data(), name);
                    found = true;
                    return true;
                }
                return false;
            });
            if (!found)
                Logging.Log("[albw] %s not found in any game module", label);
            return found;
        }
    }

    const char* ResolveSocketApi(SocketApi& api) {
        api = SocketApi {};
        Lookup(api.Initialize,   "nn::socket::Initialize",   InitializeNames);
        Lookup(api.GetLastErrno, "nn::socket::GetLastErrno", GetLastErrnoNames);
        if (!Lookup(api.Socket,  "nn::socket::Socket",  SocketNames))  return "nn::socket::Socket";
        if (!Lookup(api.Connect, "nn::socket::Connect", ConnectNames)) return "nn::socket::Connect";
        if (!Lookup(api.Send,    "nn::socket::Send",    SendNames))    return "nn::socket::Send";
        if (!Lookup(api.Recv,    "nn::socket::Recv",    RecvNames))    return "nn::socket::Recv";
        return nullptr;
    }

    void LogSocketExports() {
        constexpr int MaxListed = 60;
        int total = 0;
        ForEachGameModule([&](const elf::SymbolTable& table, const exl::util::ModuleInfo& info) {
            auto mod = info.GetModuleName();
            int n = table.ForEachWithPrefix("_ZN2nn6socket", MaxListed - total, [&](const char* name) {
                Logging.Log("[albw] socket export (%.*s): %s", static_cast<int>(mod.size()), mod.data(), name);
            });
            total += n;
            return false;   /* keep going through every module */
        });
        Logging.Log("[albw] game exports %d nn::socket function(s)%s", total,
                    total > MaxListed ? " (list cut short)" : "");
    }
}
