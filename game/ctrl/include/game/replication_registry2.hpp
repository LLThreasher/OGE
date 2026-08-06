// #pragma once

// #include <array>
// #include <climits>
// #include <concepts>
// #include <cstddef>
// #include <cstdint>
// #include <limits>
// #include <tuple>
// #include <type_traits>
// #include <vector>

// #include "entt/entity/fwd.hpp"
// #include "game/components.hpp"
// #include "game/components_net.hpp"
// #include "game/input/entity_event_stream.hpp"
// #include "game/input/net.hpp"
// #include "game/terrain/terrain_view.hpp"
// #include "oge/event_stream.hpp"
// #include "oge/event_stream2.hpp"
// #include "oge/log.hpp"
// #include "oge/runtime/net_packet_sender.hpp"
// #include "oge/runtime/net_serializer.hpp"
// #include "oge/runtime/net_server.hpp"
// #include "oge/runtime/net_traits.hpp"
// #include "oge/runtime/typed_registry.hpp"


// namespace game
// {
// using oge::runtime::FamilyId;
// using oge::runtime::ICapability;
// using oge::runtime::oge_id_type;
// using oge::runtime::SendType;
// using oge::runtime::TypeRegistry;
// namespace net = oge::runtime::net;
// using input::EntityEvent;
// using input::EntityEventStream;
// using input::EntityEventType;
// using oge::DiscreteEventStream;

// using NetCursor = uint32_t;

// template <typename T>
// concept IsInputStream =
//     requires {
//         typename T::TEvent;
//         typename T::Cursor;
//     } && std::same_as<typename T::Cursor, NetCursor> &&
//     requires(const T& s, typename T::Cursor& c, typename T::TEvent& e) {
//         { s.PollOne(c, e) } -> std::same_as<bool>;
//         { s.AdvanceCursor(c) };
//     };

// template <typename T>
// concept IsOutputStream =
//     requires {
//         typename T::TEvent;
//         typename T::Cursor;
//     } && std::same_as<typename T::Cursor, NetCursor> &&
//     requires(T& s, typename T::Cursor c, const typename T::TEvent& e) {
//         { s.Insert(c, e) } -> std::same_as<bool>;
//     };

// struct EncodeContext
// {
//     NetCursor begin{};
//     size_t cnt = std::numeric_limits<size_t>::max();
//     ENetPeer* peer = nullptr;
//     size_t maxPacketBytes = 1200;
//     uint64_t sendTime = 0;
// };

// struct DecodeContext
// {
//     NetCursor begin = 0;
//     size_t cnt = 0;
//     ENetPeer* peer = nullptr;
//     uint8_t channel = 0;
//     uint64_t receiveTime = 0;
// };

// struct PacketPlan
// {
//     bool hasPacket = false;
//     NetCursor end{};
//     size_t byteCount = 0;
//     uint32_t eventCount = 0;
// };

// template <typename TIS, typename TOS, typename TStreamEncoder>
// struct RegistryStreamEncoder
// {
//     using IS = TIS;
//     using OS = TOS;

//     static PacketPlan Prepare(const entt::registry& registry,
//                               const EncodeContext& ctx)
//     {
//         const IS& input = registry.ctx().get<IS>();
//         return TStreamEncoder::Prepare(input, ctx);
//     }

//     static bool Encode(const entt::registry& registry, const EncodeContext& ctx,
//                        net::Buffer& buf)
//     {
//         const IS& input = registry.ctx().get<IS>();
//         return TStreamEncoder::Encode(input, ctx, buf);
//     }

//     static bool Decode(entt::registry& registry, const DecodeContext& ctx,
//                        net::Buffer& buf)
//     {
//         OS& output = registry.ctx().get<OS>();
//         return TStreamEncoder::Decode(output, ctx, buf);
//     }
// };

// template <typename T>
// concept IsNetStreamEncoder =
//     requires {
//         typename T::IS;
//         typename T::OS;
//     } && IsInputStream<typename T::IS> && IsOutputStream<typename T::OS> &&
//     requires(const typename T::IS& is, typename T::OS& os, NetCursor begin,
//              NetCursor end, const EncodeContext& encodeCtx,
//              const DecodeContext& decodeCtx, net::Buffer& buffer) {
//         { T::Prepare(is, begin, encodeCtx) } -> std::same_as<PacketPlan>;
//         { T::Encode(is, begin, end, encodeCtx, buffer) } -> std::same_as<bool>;
//         { T::Decode(os, decodeCtx, buffer) } -> std::same_as<bool>;
//     };

// struct ReplicationCapability : ICapability
// {
//     using PrepareFn = PacketPlan (*)(const entt::registry&,
//                                      const EncodeContext&);
//     using EncodeFn = bool (*)(const entt::registry&, const EncodeContext&,
//                               net::Buffer&);
//     using DecodeFn = bool (*)(entt::registry&, const DecodeContext&,
//                               net::Buffer&);

//     FamilyId family{};
//     SendType sendType = SendType::Unreliable;
//     uint8_t channel = 0;

//     PrepareFn prepare = nullptr;
//     EncodeFn encode = nullptr;
//     DecodeFn decode = nullptr;

//     template <typename TEncoder>
//         requires IsNetStreamEncoder<TEncoder>
//     static ReplicationCapability Make(FamilyId f,
//                                       SendType st = SendType::Unreliable,
//                                       uint8_t ch = 0)
//     {
//         ReplicationCapability cap{};
//         cap.family = f;
//         cap.sendType = st;
//         cap.channel = ch;
//         cap.prepare = &TEncoder::Prepare;
//         cap.encode = &TEncoder::Encode;
//         cap.decode = &TEncoder::Decode;
//         return cap;
//     }
// };

// template <typename TIS, typename TOS>
//     requires IsInputStream<TIS> && IsOutputStream<TOS> &&
//              std::same_as<typename TIS::TEvent, typename TOS::TEvent>
// struct SimpleEventEncoder
// {
//     using IS = TIS;
//     using OS = TOS;
//     using T = typename IS::TEvent;

//     static PacketPlan Prepare(const IS& s, const EncodeContext& ctx)
//     {
//         PacketPlan plan{};
//         plan.end = ctx.begin;

//         T event{};
//         size_t cnt = ctx.cnt;

//         while (cnt > 0 && s.PollOne(plan.end, event))
//         {
//             const auto eventSize = net::Size(event);

//             if (plan.byteCount + eventSize > ctx.maxPacketBytes)
//             {
//                 break;
//             }

//             plan.hasPacket = true;
//             ++plan.eventCount;
//             plan.byteCount += eventSize;
//             --cnt;

//             if (plan.eventCount >= std::numeric_limits<uint8_t>::max())
//             {
//                 break;
//             }
//         }

//         return plan;
//     }

//     static bool Encode(const IS& s, const EncodeContext& ctx, net::Buffer& buf)
//     {
//         NetCursor at = ctx.begin;
//         size_t cnt = ctx.cnt;

//         while (cnt > 0)
//         {
//             T event{};

//             if (!s.PollOne(at, event))
//             {
//                 return false;
//             }

//             net::Serialize(buf, event);
//             --cnt;
//         }

//         return true;
//     }

//     static bool Decode(OS& s, const DecodeContext& ctx, net::Buffer& buf)
//     {
//         NetCursor at = ctx.begin;
//         size_t cnt = ctx.cnt;

//         while (cnt > 0)
//         {
//             if (buf.IsEmpty())
//             {
//                 return false;
//             }

//             T event{};
//             net::Deserialize(buf, event);

//             s.Insert(at, event);

//             ++at;
//             --cnt;
//         }

//         return buf.IsEmpty();
//     }
// };

// class ReplicationRegistry
// {
//     struct PeerState
//     {
//         std::unordered_map<FamilyId, NetCursor> cursor;
//     };

//     std::vector<oge_id_type> m_serializableComponents;
//     std::vector<FamilyId> m_sendUnits;
//     std::unordered_map<FamilyId, ReplicationCapability*> m_units;
//     std::unordered_map<ENetPeer*, PeerState> m_peers;

//    public:
//     const std::vector<oge_id_type>& SeralizableComponents() const
//     {
//         return m_serializableComponents;
//     }

//     template <typename T>
//     void RegisterSerializableComponent()
//     {
//         m_serializableComponents.push_back(entt::type_hash<T>::value());
//     }

//     void AddFamilyToSend(FamilyId id)
//     {
//         m_sendUnits.push_back(id);
//     }

//     void RegisterFrom(TypeRegistry& types)
//     {
//         for (auto& type : types.GetAll())
//         {
//             if (auto* cap = type.capabilities.Get<ReplicationCapability>())
//             {
//                 m_units[cap->family] = cap;
//             }
//         }
//     }

//     void ProduceAll(oge::runtime::NetPacketSender& server,
//                     entt::registry& world)
//     {
//         for (auto& [peer, peerState] : m_peers)
//         {
//             for (auto family : m_sendUnits)
//             {
//                 auto* cap = m_units.at(family);

//                 auto& cursor = peerState.cursor[family];

//                 EncodeContext ectx{};
//                 ectx.peer = peer;
//                 ectx.begin = cursor;
//                 ectx.maxPacketBytes = 1200 - sizeof(FamilyId) -
//                                       sizeof(NetCursor) - sizeof(uint8_t);

//                 auto plan = cap->prepare(world, ectx);
//                 if (!plan.hasPacket)
//                 {
//                     continue;
//                 }

//                 assert(plan.eventCount <= std::numeric_limits<uint8_t>::max());

//                 auto buf =
//                     server.StartPacket(sizeof(FamilyId) + sizeof(NetCursor) +
//                                        sizeof(uint8_t) + plan.byteCount);

//                 buf.Write<FamilyId>(family);
//                 buf.Write<NetCursor>(ectx.begin);
//                 buf.Write<uint8_t>(static_cast<uint8_t>(plan.eventCount));

//                 ectx.cnt = plan.eventCount;

//                 assert(cap->encode(world, ectx, buf));

//                 server.Send(peer, buf, cap->sendType, cap->channel);

//                 cursor = plan.end;
//             }
//         }
//     }

//     void HandleIncoming(entt::registry& world, net::Buffer& buffer)
//     {
//         FamilyId family = buffer.Read<FamilyId>();
//         NetCursor begin = buffer.Read<NetCursor>();
//         uint8_t cnt = buffer.Read<uint8_t>();

//         auto it = m_units.find(family);
//         if (it == m_units.end())
//         {
//             return;
//         }

//         DecodeContext dctx{};
//         dctx.begin = begin;
//         dctx.cnt = cnt;

//         it->second->decode(world, dctx, buffer);
//     }

//     void AddPeer(ENetPeer* peer)
//     {
//         PeerState peerState;

//         for (auto& [family, cap] : m_units)
//         {
//             peerState.cursor.emplace(family);
//         }

//         m_peers.emplace(peer, std::move(peerState));
//     }

//     void RemovePeer(ENetPeer* peer)
//     {
//         m_peers.erase(peer);  // entt::any cleans itself
//     }

//     auto Peers() const
//     {
//         return m_peers;
//     }
// };

// using EntityNetStream =
//     oge::NetworkEventStream<input::EntityEvent>;

// using EntityNetStreamEncoder =
//     RegistryStreamEncoder<
//         EntityNetStream,
//         EntityNetStream,
//         SimpleEventEncoder<EntityNetStream, EntityNetStream>>;

// void InstallEntityReplicationHooks(entt::registry& world);
// void ApplyEntityReplicationEvents(entt::registry& world);

// template <typename T>
// struct ComponentReplicationEvent
// {
//     input::ComponentDeltaType type{};
//     entt::entity entity{};
//     T value{};
// };

// template <typename T>
// using ComponentNetStream =
//     oge::NetworkEventStream<ComponentReplicationEvent<T>>;

// template <typename T>
// using ComponentNetStreamEncoder =
//     RegistryStreamEncoder<
//         ComponentNetStream<T>,
//         ComponentNetStream<T>,
//         SimpleEventEncoder<ComponentNetStream<T>, ComponentNetStream<T>>>;

// template <typename T>
// void InstallComponentReplicationHooks(entt::registry& world)
// {
//     world.ctx().emplace<ComponentNetStream<T>>();

//     world.on_construct<T>()
//         .template connect<
//             +[](entt::registry& world, entt::entity e)
//             {
//                 world.ctx()
//                     .template get<ComponentNetStream<T>>()
//                     .Push({
//                         input::ComponentDeltaType::Add,
//                         e,
//                         world.get<T>(e)
//                     });
//             }>();

//     world.on_update<T>()
//         .template connect<
//             +[](entt::registry& world, entt::entity e)
//             {
//                 world.ctx()
//                     .template get<ComponentNetStream<T>>()
//                     .Push({
//                         input::ComponentDeltaType::Update,
//                         e,
//                         world.get<T>(e)
//                     });
//             }>();

//     world.on_destroy<T>()
//         .template connect<
//             +[](entt::registry& world, entt::entity e)
//             {
//                 world.ctx()
//                     .template get<ComponentNetStream<T>>()
//                     .Push({
//                         input::ComponentDeltaType::Remove,
//                         e,
//                         {}
//                     });
//             }>();
// }

// template <typename T>
// void ApplyComponentReplicationEvents(entt::registry& world)
// {
//     auto* stream = world.ctx().find<ComponentNetStream<T>>();
//     if (!stream)
//     {
//         return;
//     }

//     static typename ComponentNetStream<T>::Cursor cursor = 0;

//     ComponentReplicationEvent<T> event{};

//     while (stream->PollOne(cursor, event))
//     {
//         switch (event.type)
//         {
//             case input::ComponentDeltaType::Add:
//             case input::ComponentDeltaType::Update:
//             {
//                 if (!world.valid(event.entity))
//                 {
//                     world.create(event.entity);
//                 }

//                 if (!world.all_of<T>(event.entity))
//                 {
//                     world.emplace<T>(event.entity, std::move(event.value));
//                 }
//                 else
//                 {
//                     world.replace<T>(event.entity, std::move(event.value));
//                     world.patch<T>(event.entity);
//                 }

//                 break;
//             }

//             case input::ComponentDeltaType::Remove:
//             {
//                 if (world.valid(event.entity) && world.all_of<T>(event.entity))
//                 {
//                     world.remove<T>(event.entity);
//                 }

//                 break;
//             }
//         }
//     }
// }

// template <typename TEvent>
// using GenericReplicationStream = oge::NetworkEventStream<TEvent>;

// template <typename T>
// struct ComponentReplication
// {
//     struct State
//     {
//         typename input::ComponentDeltaStream<T>::Cursor cursor{};
//         bool needsSnapshot = true;
//     };

//     static void SendSnapshot(entt::registry& world, ENetPeer* peer,
//                              oge::runtime::NetPacketSender& server,
//                              FamilyId family, SendType sendType,
//                              uint8_t channel, State& state)
//     {
//         auto view = world.view<T>();

//         for (auto entity : view)
//         {
//             size_t size = sizeof(FamilyId) + sizeof(input::ComponentDeltaType) +
//                           sizeof(entt::entity) +
//                           net::Size(world.get<T>(entity));

//             auto packet = server.StartPacket(size);

//             packet.Write(family);
//             packet.Write(input::ComponentDeltaType::Add);
//             packet.Write(entity);

//             net::Serialize(packet, world.get<T>(entity));

//             server.Send(peer, packet, sendType, channel);
//         }

//         state.needsSnapshot = false;
//     }

//     static entt::any CreateState()
//     {
//         return State{};
//     }

//     static void Encode(entt::registry& world, ENetPeer* peer,
//                        oge::runtime::NetPacketSender& server, FamilyId family,
//                        SendType sendType, uint8_t channel, entt::any& anyState)
//     {
//         // LOG_DEBUG("encode {}", oge::runtime::TypeName<T>::Get());
//         auto& state = entt::any_cast<State&>(anyState);

//         auto* stream = world.ctx().find<input::ComponentDeltaStream<T>>();
//         if (!stream) return;

//         if (state.needsSnapshot)
//         {
//             stream->AdvanceCursor(state.cursor);
//             SendSnapshot(world, peer, server, family, sendType, channel, state);
//             return;
//         }

//         entt::sparse_set newComps;
//         entt::storage<T> storage;
//         input::ComponentDeltaEvent<T> delta;
//         while (stream->PollOne(state.cursor, delta))
//         {
//             if (!world.valid(delta.entity)) continue;
//             if (delta.type != input::ComponentDeltaType::Remove)
//             {
//                 if (!storage.contains(delta.entity))
//                     storage.emplace(delta.entity, world.get<T>(delta.entity));
//                 if (delta.type == input::ComponentDeltaType::Add)
//                     newComps.push(delta.entity);
//             }
//             else
//             {
//                 if (storage.contains(delta.entity))
//                 {
//                     storage.erase(delta.entity);
//                 }
//                 if (newComps.contains(delta.entity))
//                 {
//                     newComps.erase(delta.entity);
//                 }
//                 else
//                 {
//                     size_t size = sizeof(FamilyId) +
//                                   sizeof(input::ComponentDeltaType) +
//                                   sizeof(entt::entity);
//                     auto packet = server.StartPacket(size);
//                     packet.Write(family);
//                     packet.Write(delta.type);
//                     packet.Write(delta.entity);
//                     server.Send(peer, packet, sendType, channel);
//                 }
//             }
//         }

//         for (auto [e, data] : storage.each())
//         {
//             auto ty = newComps.contains(e) ? input::ComponentDeltaType::Add
//                                            : input::ComponentDeltaType::Update;
//             size_t size = sizeof(FamilyId) + sizeof(input::ComponentDeltaType) +
//                           sizeof(entt::entity) +
//                           net::Size(world.get<T>(delta.entity));

//             auto packet = server.StartPacket(size);

//             packet.Write(family);
//             packet.Write(delta.type);
//             packet.Write(delta.entity);
//             net::Serialize(packet, world.get<T>(delta.entity));
//             server.Send(peer, packet, sendType, channel);
//         }
//     }

//     static void Decode(entt::registry& world, net::Buffer& buffer)
//     {
//         input::ComponentDeltaType type;
//         buffer.Read(type);

//         entt::entity entity;
//         buffer.Read(entity);

//         // LOG_DEBUG("decode {} for {}", oge::runtime::TypeName<T>::Get(),
//         // (uint64_t)entity);

//         switch (type)
//         {
//             case input::ComponentDeltaType::Add:
//             case input::ComponentDeltaType::Update:
//             {
//                 if (!world.all_of<T>(entity))
//                 {
//                     T res{};
//                     net::Deserialize(buffer, res);
//                     world.emplace<T>(entity, std::move(res));  // this line
//                 }
//                 else
//                 {
//                     T& res = world.get<T>(entity);
//                     net::Deserialize(buffer, res);
//                     world.patch<T>(entity);
//                 }
//                 break;
//             }

//             case input::ComponentDeltaType::Remove:
//             {
//                 if (world.valid(entity) && world.all_of<T>(entity))
//                     world.remove<T>(entity);
//                 break;
//             }
//         }
//     }
// };

// void InstallTerrainReplicationHooks(entt::registry& world);

// struct TerrainReplication
// {
//     struct State
//     {
//         terrain::ChunkHandle snapshotCursor{};
//         bool needsSnapshot = true;
//         terrain::ChunkEventStream::Cursor chunkEventCursor{};
//     };

//     static entt::any CreateState();
//     static void Encode(entt::registry& world, ENetPeer* peer,
//                        oge::runtime::NetPacketSender& server, FamilyId family,
//                        SendType sendType, uint8_t channel, entt::any& anyState);
//     static void Decode(entt::registry& world, net::Buffer& buffer);
// };

// template <typename T>
// concept IsEventStream = requires(T s, T::Cursor c, T::TEvent e) {
//     typename T::TEvent;
//     typename T::Cursor;
//     { s.AdvanceCursor(c) };
//     { s.PollOne(c, e) } -> std::same_as<bool>;
//     { s.Push(e) } -> std::same_as<void>;
// };

// template <typename TEventStream>
//     requires IsEventStream<TEventStream>
// struct EventStreamReplication
// {
//     using TEvent = TEventStream::TEvent;
//     struct State
//     {
//         typename TEventStream::Cursor cursor = 0;
//         bool initialized = false;
//     };

//     static entt::any CreateState()
//     {
//         return State{};
//     }

//     static void Encode(entt::registry& world, ENetPeer* peer,
//                        oge::runtime::NetPacketSender& server, FamilyId family,
//                        SendType sendType, uint8_t channel, entt::any& anyState)
//     {
//         auto& state = entt::any_cast<State&>(anyState);
//         auto& stream = world.ctx().get<TEventStream>();

//         if (!state.initialized)
//         {
//             stream.AdvanceCursor(state.cursor);
//             state.initialized = true;
//             return;
//         }

//         NetEventBatch<TEvent> batch;

//         TEvent ev;
//         while (stream.PollOne(state.cursor, ev))
//             batch.events.Add(std::move(ev));

//         if (batch.events.empty()) return;

//         auto packet = server.StartPacket(sizeof(FamilyId) + net::Size(batch));

//         packet.Write(family);
//         net::Serialize(packet, batch);

//         server.Send(peer, packet, sendType, channel);
//     }

//     static void Decode(entt::registry& world, net::Buffer& buffer)
//     {
//         NetEventBatch<TEvent> batch;
//         net::Deserialize(buffer, batch);

//         auto& queue = world.ctx().get<TEventStream>();

//         for (auto& e : batch.events) queue.Push(std::move(e));
//     }
// };

// template <typename TEventStream>
//     requires IsEventStream<TEventStream>
// struct EntityEventStreamReplication
// {
//     using TEvent = typename TEventStream::TEvent;

//     struct PerStreamState
//     {
//         typename TEventStream::Cursor cursor{};
//         bool initialized = false;
//     };

//     struct State
//     {
//         std::unordered_map<entt::entity, PerStreamState> perStreamStates;
//         EntityEventStream::Cursor eCursor;
//     };

//     static entt::any CreateState()
//     {
//         LOG_DEBUG("create e event state");
//         return State{};
//     }

//     static void Encode(entt::registry& world, ENetPeer* peer,
//                        oge::runtime::NetPacketSender& server, FamilyId family,
//                        SendType sendType, uint8_t channel, entt::any& anyState)
//     {
//         auto& state = entt::any_cast<State&>(anyState);

//         EntityEvent ee;
//         auto eStream = world.ctx().find<EntityEventStream>();
//         if (!eStream) return;
//         while (eStream->PollOne(state.eCursor, ee))
//         {
//             if (ee.type == EntityEventType::Destroy)
//             {
//                 state.perStreamStates.erase(ee.entity);
//             }
//         }

//         auto view = world.view<TEventStream>();

//         for (auto entity : view)
//         {
//             auto& stream = world.get<TEventStream>(entity);

//             PerStreamState& pState = state.perStreamStates[entity];

//             if (!pState.initialized)
//             {
//                 stream.AdvanceCursor(pState.cursor);
//                 pState.initialized = true;
//                 continue;
//             }

//             NetEventBatch<TEvent> batch;

//             TEvent ev;
//             while (stream.PollOne(pState.cursor, ev))
//                 batch.push_back(std::move(ev));

//             if (batch.empty()) continue;

//             auto packet = server.StartPacket(
//                 sizeof(FamilyId) + sizeof(entt::entity) + net::Size(batch));

//             packet.Write(family);
//             packet.Write(entity);
//             net::Serialize(packet, batch);

//             server.Send(peer, packet, sendType, channel);
//         }
//     }

//     static void Decode(entt::registry& world, net::Buffer& buffer)
//     {
//         auto entity = buffer.Read<entt::entity>();

//         NetEventBatch<TEvent> batch;
//         net::Deserialize(buffer, batch);

//         auto& queue = world.get<TEventStream>(entity);

//         for (auto& e : batch) queue.Push(std::move(e));
//     }
// };

// void RegisterReplications(oge::runtime::AnythingFactory& af,
//                           ReplicationRegistry& rf);
// }  // namespace game

// namespace oge::runtime::net
// {
// using game::ComponentReplicationEvent;
// namespace input = game::input;

// template <typename T>
// inline std::size_t Size(const ComponentReplicationEvent<T>& event)
// {
//     std::size_t size =
//         sizeof(input::ComponentDeltaType) +
//         sizeof(entt::entity);

//     if (event.type != input::ComponentDeltaType::Remove)
//     {
//         size += Size(event.value);
//     }

//     return size;
// }

// template <typename T>
// inline void Serialize(Buffer& buf, const ComponentReplicationEvent<T>& event)
// {
//     buf.Write(event.type);
//     buf.Write(event.entity);

//     if (event.type != input::ComponentDeltaType::Remove)
//     {
//         Serialize(buf, event.value);
//     }
// }

// template <typename T>
// inline void Deserialize(Buffer& buf, ComponentReplicationEvent<T>& event)
// {
//     buf.Read(event.type);
//     buf.Read(event.entity);

//     if (event.type != input::ComponentDeltaType::Remove)
//     {
//         Deserialize(buf, event.value);
//     }
// }
// }
