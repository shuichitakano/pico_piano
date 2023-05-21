/*
 * author : Shuichi TAKANO
 * since  : Sat Apr 15 2023 17:09:34
 */

#include "ble_client_manager.h"
#include <tuple>
#include <utility>
#include <array>
#include <cstdio>
#include <optional>
#include <inttypes.h>

namespace bluetooth
{

    struct ClientEventHandlerInfo
    {
        BLEClientManager *man{};
        BLEClientManager::Connection *conn{};
        btstack_packet_handler_t handler{};
    };

    namespace
    {
        static constexpr size_t N_CLIENT_EVENT_HANDLERS = 4;

        std::array<ClientEventHandlerInfo, N_CLIENT_EVENT_HANDLERS> clientEventHandlerInfos_;

        template <int i>
        struct ClientEventHandlerEntry
        {
            static void
            onEvent(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
            {
                auto &hi = clientEventHandlerInfos_[i];
                hi.man->onGATTEvent(hi.conn, packet_type, channel, packet, size);
            }
        };

        struct ClientEventHandlerAllocator
        {
            ClientEventHandlerAllocator()
            {
                init(std::make_index_sequence<N_CLIENT_EVENT_HANDLERS>());
            }

            template <size_t... Seq>
            void init(std::index_sequence<Seq...>)
            {
                btstack_packet_handler_t handlers[] = {ClientEventHandlerEntry<Seq>::onEvent...};
                for (auto i = 0u; i < N_CLIENT_EVENT_HANDLERS; ++i)
                {
                    clientEventHandlerInfos_[i].handler = handlers[i];
                }
            }

            ClientEventHandlerInfo *allocate(BLEClientManager *m)
            {
                for (auto &info : clientEventHandlerInfos_)
                {
                    if (!info.man)
                    {
                        info.man = m;
                        return &info;
                    }
                }
                return nullptr;
            }

            void free(ClientEventHandlerInfo *p)
            {
                p->man = nullptr;
            }
        };

        void HCIEventHandlerEntry(uint8_t packet_type,
                                  uint16_t channel, uint8_t *packet,
                                  uint16_t size)
        {
            BLEClientManager::instance().onHCIEvent(packet_type, channel, packet, size);
        }

        static ClientEventHandlerAllocator clientEventHandlerAllocator_;

        std::string makeUUIDString(uint16_t uuid16, const uint8_t uuid128[16])
        {
            if (uuid16)
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "%04x", uuid16);
                return {buf};
            }
            else
            {
                return uuid128_to_str(uuid128);
            }
        }

        std::optional<std::string>
        parseAdvDataServiceID128(const uint8_t *adv_data, size_t adv_size)
        {
            ad_context_t context;
            bd_addr_t address;
            for (ad_iterator_init(&context, adv_size, (uint8_t *)adv_data); ad_iterator_has_more(&context); ad_iterator_next(&context))
            {
                uint8_t data_type = ad_iterator_get_data_type(&context);
                uint8_t size = ad_iterator_get_data_len(&context);
                const uint8_t *data = ad_iterator_get_data(&context);
                switch (data_type)
                {
                case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
                case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
                case BLUETOOTH_DATA_TYPE_LIST_OF_128_BIT_SERVICE_SOLICITATION_UUIDS:
                {
                    uint8_t uuid_128[16];
                    reverse_128(data, uuid_128);
                    return uuid128_to_str(uuid_128);
                }
                }
            }
            return {};
        }

        std::optional<std::string>
        parseAdvDataName(const uint8_t *adv_data, size_t adv_size)
        {
            ad_context_t context;
            bd_addr_t address;
            for (ad_iterator_init(&context, adv_size, (uint8_t *)adv_data); ad_iterator_has_more(&context); ad_iterator_next(&context))
            {
                uint8_t data_type = ad_iterator_get_data_type(&context);
                uint8_t size = ad_iterator_get_data_len(&context);
                const uint8_t *data = ad_iterator_get_data(&context);
                switch (data_type)
                {
                case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
                case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                    return reinterpret_cast<const char *>(data);
                }
            }
            return {};
        }

    }

    ////////////////////////////////////
    ////////////////////////////////////
    bool
    BLEClientHandler::write(int valueHandle, const uint8_t *data, size_t size, bool needResponse) const
    {
        if (conHandle_)
        {
            if (needResponse)
            {
                return ERROR_CODE_SUCCESS == gatt_client_write_value_of_characteristic_without_response(conHandle_, valueHandle, size, const_cast<uint8_t *>(data));
            }
            else
            {
                return ERROR_CODE_SUCCESS == gatt_client_write_value_of_characteristic(packetHandler_, conHandle_, valueHandle, size, const_cast<uint8_t *>(data));
            }
        }
        return false;
    }

    void
    BLEClientHandler::_set(hci_con_handle_t h, btstack_packet_handler_t handler)
    {
        conHandle_ = h;
        packetHandler_ = handler;
    }

    ////////////////////////////////////
    ////////////////////////////////////

    void
    AdvertisingReport::import(const uint8_t *packet)
    {
        gap_event_advertising_report_get_address(packet, address_.data());
        eventType_ = gap_event_advertising_report_get_advertising_event_type(packet);
        addressType_ = static_cast<bd_addr_type_t>(gap_event_advertising_report_get_address_type(packet));
        rssi_ = (int8_t)gap_event_advertising_report_get_rssi(packet);

        auto length = gap_event_advertising_report_get_data_length(packet);
        auto data = gap_event_advertising_report_get_data(packet);

        if (auto name = parseAdvDataName(data, length))
        {
            name_ = name.value();
        }

        if (auto uuid = parseAdvDataServiceID128(data, length))
        {
            uuid_ = uuid.value();
        }
    }

    void
    AdvertisingReport::dump() const
    {
        printf("  * adv. event: evt-type %u, addr-type %u, addr %s, rssi %d, name %s, uuid %s\n",
               eventType_,
               addressType_, bd_addr_to_str(address_.data()), rssi_, name_.c_str(), uuid_.c_str());
    }

    ////////////////////////////////////
    ////////////////////////////////////
    Service::Service(const gatt_client_service_t &obj)
        : obj_(obj)
    {
        uuid_ = makeUUIDString(obj_.uuid16, obj_.uuid128);
        printf(" * service: [0x%04x-0x%04x], uuid %s\n", obj_.start_group_handle, obj_.end_group_handle, uuid_.c_str());
    }

    ////////////////////////////////////
    ////////////////////////////////////

    Characteristic::Characteristic(const gatt_client_characteristic_t &obj)
        : obj_(obj)
    {
        uuid_ = makeUUIDString(obj_.uuid16, obj_.uuid128);
#if 1
        printf("  * characteristic: [0x%04x-0x%04x-0x%04x], properties 0x%02x, uuid %s\n",
               obj_.start_handle, obj_.value_handle, obj_.end_handle, obj_.properties, uuid_.c_str());

        if (hasBroadcastProp())
            printf("broadcast ");
        if (hasReadProp())
            printf("read ");
        if (hasWriteWithoutResponseProp())
            printf("writeNR ");
        if (hasWriteProp())
            printf("write ");
        if (hasNotifyProp())
            printf("notify ");
        if (hasIndicateProp())
            printf("indicate ");
        if (hasSignedWriteProp())
            printf("signedWrite ");
        if (hasExtendedProperties())
            printf("ext ");
        printf("\n");
#endif
    }

    bool
    Characteristic::_start(btstack_packet_handler_t handler, hci_con_handle_t conHandle)
    {
        int cfg = GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NONE;
        if (hasNotifyProp())
        {
            cfg = GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
        }
        else if (hasIndicateProp())
        {
            cfg = GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION;
        }

        gatt_client_listen_for_characteristic_value_updates(&notificationListener_, handler, conHandle, &obj_);
        auto r = gatt_client_write_client_characteristic_configuration(handler, conHandle, &obj_, cfg);
        return r == ERROR_CODE_SUCCESS;

        // 以後Characteristic のアドレスが変わってはいけない
    }

    ////////////////////////////////////
    ////////////////////////////////////

    BLEClientManager::Connection::Connection(ClientEventHandlerInfo *hinfo, hci_con_handle_t conHandle, BLEClientHandler *h)
        : clientHandler_(h), eventHandlerInfo_(hinfo), conHandle_(conHandle)
    {
        hinfo->conn = this;
        clientHandler_->_set(conHandle, hinfo->handler);
    }

    BLEClientManager::Connection::~Connection()
    {
        clientHandler_->onDisconnect();
        clientHandler_->_set({}, {});
        clientEventHandlerAllocator_.free(eventHandlerInfo_);
    }

    void
    BLEClientManager::Connection::startSearchService()
    {
        currentHandler_ = &Connection::searchServiceHandler;
        gatt_client_discover_primary_services(eventHandlerInfo_->handler, conHandle_);
    }

    bool
    BLEClientManager::Connection::searchNextCharacteristics()
    {
        ++discoveringService_;
        if (discoveringService_ >= services_.size())
        {
            return false;
        }
        auto &service = services_[discoveringService_];

        currentHandler_ = &Connection::searchCharacteristicsHandler;
        gatt_client_discover_characteristics_for_service(eventHandlerInfo_->handler, conHandle_,
                                                         const_cast<gatt_client_service_t *>(&service.getRaw()));
        return true;
    }

    void
    BLEClientManager::Connection::startValueUpdate()
    {
        currentHandler_ = &Connection::notifyHandler;
        for (auto &service : services_)
        {
            for (int i = 0; i < service.getCharacteristicsCount(); ++i)
            {
                auto &chr = service.getCharacteristics(i);
                if (chr._isEnabled())
                {
                    chr._start(eventHandlerInfo_->handler, conHandle_);
                }
            }
        }
    }

    void
    BLEClientManager::Connection::onGATTEvent(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size)
    {
        if (currentHandler_)
        {
            if (!(this->*currentHandler_)(packet_type, channel, packet, size))
            {
                auto event = hci_event_packet_get_type(packet);
                switch (event)
                {
                case GATT_EVENT_MTU:
                    printf(" MTU %d\n", gatt_event_mtu_get_MTU(packet));
                    break;

                default:
                    printf("Unhandled GATT client event. %02x\n", event);
                }
            }
        }
    }

    bool
    BLEClientManager::Connection::checkQueryCompleteStatus(const uint8_t *packet) const
    {
        auto status = gatt_event_query_complete_get_att_status(packet);
        switch (status)
        {
        case ATT_ERROR_SUCCESS:
            // printf("GATT Query result: OK\n");
            return true;

        case ATT_ERROR_INSUFFICIENT_ENCRYPTION:
            printf("GATT Query result: Insufficient Encryption\n");
            // sm_request_pairing(connection_handler_);
            break;
        case ATT_ERROR_INSUFFICIENT_AUTHENTICATION:
            printf("GATT Query result: Insufficient Authentication\n");
            break;
        case ATT_ERROR_BONDING_INFORMATION_MISSING:
            printf("GATT Query result: Bonding Information Missing\n");
            break;
        case ATT_ERROR_TIMEOUT:
            printf("GATT Query result: Timeout\n");
            return true;
        case ATT_ERROR_HCI_DISCONNECT_RECEIVED:
            printf("GATT Query result: HCI Disconnect Received\n");
            break;

        default:
            printf("GATT Query result: 0x%02x\n", gatt_event_query_complete_get_att_status(packet));
            break;
        }
        return false;
    }

    bool
    BLEClientManager::Connection::searchServiceHandler(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size)
    {
        switch (hci_event_packet_get_type(packet))
        {
        case GATT_EVENT_SERVICE_QUERY_RESULT:
        {
            gatt_client_service_t service;
            gatt_event_service_query_result_get_service(packet, &service);

            services_.emplace_back(service);
            return true;
        }

        case GATT_EVENT_QUERY_COMPLETE:
        {
            if (checkQueryCompleteStatus(packet))
            {
                resetDiscoveringCharacteristicServiceIndex();
                searchNextCharacteristics();
            }
            return true;
        }
        }
        return false;
    }

    bool
    BLEClientManager::Connection::searchCharacteristicsHandler(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size)
    {
        switch (hci_event_packet_get_type(packet))
        {
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
        {
            gatt_client_characteristic_t chr;
            gatt_event_characteristic_query_result_get_characteristic(packet, &chr);

            assert(discoveringService_ < services_.size());
            auto &service = services_[discoveringService_];
            service.addCharacteristics({chr});

            auto &lchr = service.getLatestCharacteristics();
            printf("service[%d] %s, characteristic %s, prop %d\n",
                   discoveringService_,
                   service.getUUIDString().c_str(),
                   lchr.getUUIDString().c_str(), lchr.getProperties());
            if (clientHandler_->onEnumServiceCharacteristic(service, lchr))
            {
                printf("   enabled!\n");
                lchr._setEnable();
            }
            return true;
        }

        case GATT_EVENT_QUERY_COMPLETE:
        {
            if (checkQueryCompleteStatus(packet))
            {
                if (!searchNextCharacteristics())
                {
                    printf("search characteristic done.\n");
                    startValueUpdate();
                }
            }
            return true;
        }
        }
        return false;
    }

    bool
    BLEClientManager::Connection::notifyHandler(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size)
    {
        switch (hci_event_packet_get_type(packet))
        {
        case GATT_EVENT_NOTIFICATION:
        {
            auto h = gatt_event_notification_get_value_handle(packet);
            auto n = gatt_event_notification_get_value_length(packet);
            auto data = gatt_event_notification_get_value(packet);
            // printf("%04x ", h);
            // printf_hexdump(data, n);

            clientHandler_->onNotify(data, n, h);

            // gatt_client_request_can_write_without_response_event(handle_gatt_client_event, connection_handler_);

#if 0
            static bool aa = true;
            if (aa)
            {
                gap_update_connection_parameters(conHandle_, 8, 12, 4, 200);
                aa = false;
            }
#endif
            return true;
        }

        case GATT_EVENT_QUERY_COMPLETE:
        {
            if (checkQueryCompleteStatus(packet))
            {
                printf("done.\n");
            }
            return true;
        }
        }
        return false;
    }

    ////////////////////////////////////
    ////////////////////////////////////
    void
    BLEClientManager::HandlerState::release()
    {
        connection_.reset();
        conAddress_ = {};
    }

    hci_con_handle_t
    BLEClientManager::HandlerState::getConnectionHandle() const
    {
        if (connection_)
        {
            return connection_->getConnectionHandle();
        }
        return {};
    }

    ////////////////////////////////////
    ////////////////////////////////////

    void
    BLEClientManager::registerHandler(BLEClientHandler *h)
    {
        handlers_.emplace_back(h);
    }

    void
    BLEClientManager::onGATTEvent(Connection *conn, uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size)
    {
        conn->onGATTEvent(packet_type, channel, packet, size);
    }

    void
    BLEClientManager::onHCIEvent(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
    {
        if (packet_type != HCI_EVENT_PACKET)
        {
            return;
        }

        uint8_t event = hci_event_packet_get_type(packet);
        switch (event)
        {
        case BTSTACK_EVENT_STATE:
        {
            auto state = btstack_event_state_get_state(packet);
            if (state != HCI_STATE_WORKING)
            {
                break;
            }
            printf("BTstack activated, start scanning!\n");
            gap_set_scan_params(0 /* scan_type */,
                                48 /* scan_interval */,
                                48 /* scan_window */,
                                0 /* scanning_filter_policy */);
            gap_start_scan();
        }
        break;

        case BTSTACK_EVENT_NR_CONNECTIONS_CHANGED:
            printf("BTSTACK_EVENT_NR_CONNECTIONS_CHANGED\n");
            break;

        case GAP_EVENT_ADVERTISING_REPORT:
        {
            AdvertisingReport report;
            report.import(packet);
            report.dump();

            for (auto &h : handlers_)
            {
                if (h.handler_->onUpdateAdvertisingReport(report))
                {
                    h.conAddress_ = report.getAddress();

                    gap_stop_scan();
                    gap_connect(report.getAddress().data(), report.getAddressType());
                }
            }
        }
        break;

        case HCI_EVENT_LE_META:
        {
            auto subEvent = hci_event_le_meta_get_subevent_code(packet);
            switch (subEvent)
            {
            case HCI_SUBEVENT_LE_CHANNEL_SELECTION_ALGORITHM:
                printf(" CSA: %d\n", hci_subevent_le_channel_selection_algorithm_get_channel_selection_algorithm(packet));
                break;

            case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
            {
                auto conHandle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                BDAddr addr;
                hci_subevent_le_connection_complete_get_peer_address(packet, addr.data());

                printf("  connection handle: 0x%02x\n", conHandle);

                {
                    le_connection_parameter_range_t range{};
                    range.le_conn_interval_min = 5; // 1.25ms unit?
                    range.le_conn_interval_max = 12;
                    range.le_conn_latency_min = 4; // 0.625ms unit?
                    range.le_conn_latency_max = 15;
                    range.le_supervision_timeout_min = 200; // 10ms unit?
                    range.le_supervision_timeout_max = 1000;
                    gap_set_connection_parameter_range(&range);
                }
                gap_set_connection_parameters(96, 48, 6, 7, 4, 72, 16, 48);
                // gap_update_connection_parameters(connection_handler_, 8, 12, 1, 200);
                // gap_request_connection_parameter_update(connection_handler_, 8, 12, 1, 200);

                for (auto &h : handlers_)
                {
                    if (h.conAddress_ == addr)
                    {
                        auto hinfo = clientEventHandlerAllocator_.allocate(this);
                        assert(hinfo);
                        if (hinfo)
                        {
                            h.connection_ = std::make_unique<Connection>(hinfo, conHandle, h.handler_);
                            h.connection_->startSearchService();
                        }
                        break;
                    }
                }
            }
            break;

            case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
            {
                printf("HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE\n");
                auto st = hci_subevent_le_connection_update_complete_get_status(packet);
                auto cint = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
                auto lat = hci_subevent_le_connection_update_complete_get_conn_latency(packet);
                auto to = hci_subevent_le_connection_update_complete_get_supervision_timeout(packet);
                printf(" status:%d interval:%d latency:%d timeout:%d\n", st, cint, lat, to);
            }
            break;

            case HCI_SUBEVENT_LE_READ_REMOTE_FEATURES_COMPLETE:
            case HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE:
            case HCI_SUBEVENT_LE_ADVERTISING_REPORT:
                break;

            default:
                printf("HCI_EVENT_LE_META: unhandled subevent %02x\n", subEvent);
            }
        }
        break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
        {
            printf("DISCONNECTED\n");
            auto conHandle = hci_event_disconnection_complete_get_connection_handle(packet);
            for (auto it = handlers_.begin(); it != handlers_.end();)
            {
                if (it->getConnectionHandle() == conHandle)
                {
                    it = handlers_.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            // print_log_buffer();
        }
        break;

        case HCI_EVENT_ENCRYPTION_CHANGE:
            printf("HCI_EVENT_ENCRYPTION_CHANGE\n");
            printf(" status:%d, enable:%d\n",
                   hci_event_encryption_change_get_status(packet),
                   hci_event_encryption_change_get_encryption_enabled(packet));
            break;

        case HCI_EVENT_ENCRYPTION_KEY_REFRESH_COMPLETE:
            printf("HCI_EVENT_ENCRYPTION_KEY_REFRESH_COMPLETE\n");
            printf(" status:%d\n", hci_event_encryption_key_refresh_complete_get_status(packet));
            break;

#define CASE_ONLY(x) \
    case x:          \
        break;
#define CASE_REPORT_ONLY(x)  \
    case x:                  \
        printf("%s:\n", #x); \
        break;

            CASE_ONLY(HCI_EVENT_TRANSPORT_PACKET_SENT);
            CASE_ONLY(HCI_EVENT_COMMAND_COMPLETE);
            CASE_REPORT_ONLY(HCI_EVENT_COMMAND_STATUS);
            CASE_REPORT_ONLY(HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS);
            CASE_REPORT_ONLY(L2CAP_EVENT_TRIGGER_RUN);

        default:
            printf("handle_hci_event: unhandled event %02x\n", event);
            break;
        }
    }

    BLEClientManager &
    BLEClientManager::instance()
    {
        static BLEClientManager inst;
        return inst;
    }

    ////////////////////////////////////
    ////////////////////////////////////

    constexpr uint32_t FIXED_PASSKEY = 123456U;

    void sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
    {
        if (packet_type != HCI_EVENT_PACKET)
            return;

        bd_addr_t addr;
        bd_addr_type_t addr_type;

        printf("-----------\n");

        switch (hci_event_packet_get_type(packet))
        {
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %" PRIu32 "\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            printf("Display Passkey: %" PRIu32 "\n", sm_event_passkey_display_number_get_passkey(packet));
            break;
        case SM_EVENT_PASSKEY_INPUT_NUMBER:
            printf("Passkey Input requested\n");
            printf("Sending fixed passkey %" PRIu32 "\n", (uint32_t)FIXED_PASSKEY);
            sm_passkey_input(sm_event_passkey_input_number_get_handle(packet), FIXED_PASSKEY);
            break;
        case SM_EVENT_PAIRING_STARTED:
            printf("Pairing started\n");
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            switch (sm_event_pairing_complete_get_status(packet))
            {
            case ERROR_CODE_SUCCESS:
                printf("Pairing complete, success\n");
                break;
            case ERROR_CODE_CONNECTION_TIMEOUT:
                printf("Pairing failed, timeout\n");
                break;
            case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                printf("Pairing failed, disconnected\n");
                break;
            case ERROR_CODE_AUTHENTICATION_FAILURE:
                printf("Pairing failed, authentication failure with reason = %u\n", sm_event_pairing_complete_get_reason(packet));
                // bluetooth.h の SM_REASON_RESERVED とか
                break;
            default:
                break;
            }
            break;
        case SM_EVENT_REENCRYPTION_STARTED:
            sm_event_reencryption_complete_get_address(packet, addr);
            printf("Bonding information exists for addr type %u, identity addr %s -> start re-encryption\n",
                   sm_event_reencryption_started_get_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_REENCRYPTION_COMPLETE:
            switch (sm_event_reencryption_complete_get_status(packet))
            {
            case ERROR_CODE_SUCCESS:
                printf("Re-encryption complete, success\n");
                break;
            case ERROR_CODE_CONNECTION_TIMEOUT:
                printf("Re-encryption failed, timeout\n");
                break;
            case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                printf("Re-encryption failed, disconnected\n");
                break;
            case ERROR_CODE_PIN_OR_KEY_MISSING:
                printf("Re-encryption failed, bonding information missing\n\n");
                printf("Assuming remote lost bonding information\n");
                printf("Deleting local bonding information and start new pairing...\n");
                sm_event_reencryption_complete_get_address(packet, addr);
                addr_type = (bd_addr_type_t)sm_event_reencryption_started_get_addr_type(packet);
                gap_delete_bonding(addr_type, addr);
                sm_request_pairing(sm_event_reencryption_complete_get_handle(packet));
                break;
            default:
                break;
            }
            break;

        case SM_EVENT_IDENTITY_RESOLVING_STARTED:
            printf("Identity resolving started\n");
            break;

        case SM_EVENT_IDENTITY_RESOLVING_FAILED:
            printf("Identity resolving failed\n");
            break;

        case SM_EVENT_IDENTITY_CREATED:
            printf("Identty created.\n");
            break;

        default:
            printf("Unhandled SM event: %02x\n", hci_event_packet_get_type(packet));
            break;
        }
    }

    ////////////////////
    ////////////////////
    bool initializeBluetooth(bool client, bool server,
                             io_capability_t ioCap,
                             bool needSecureConnect,
                             bool needBonding,
                             bool needMITMProtection,
                             bool needkeyPress)
    {
        l2cap_init();

        if (client)
        {
            gatt_client_init();

            static btstack_packet_callback_registration_t hci_event_callback_registration;
            hci_event_callback_registration.callback = &HCIEventHandlerEntry;
            hci_add_event_handler(&hci_event_callback_registration);
        }

        sm_init();
        sm_set_io_capabilities(ioCap);
        sm_set_authentication_requirements(
            (needSecureConnect ? SM_AUTHREQ_SECURE_CONNECTION : 0) |
            (needBonding ? SM_AUTHREQ_BONDING : 0) |
            (needMITMProtection ? SM_AUTHREQ_MITM_PROTECTION : 0) |
            (needkeyPress ? SM_AUTHREQ_KEYPRESS : 0));
        // gatt_client_set_required_security_level(LEVEL_2);
        //  sm_set_secure_connections_only_mode(true);

        static btstack_packet_callback_registration_t sm_event_callback_registration;
        sm_event_callback_registration.callback = &sm_packet_handler;
        sm_add_event_handler(&sm_event_callback_registration);

        return true;
    }

    void startBluetooth();

    ////////////////////
    ////////////////////
    void removeBondingInfoForDebug()
    {
        for (int i = 0; i < le_device_db_max_count(); ++i)
        {
            le_device_db_remove(i);
        }
    }
}