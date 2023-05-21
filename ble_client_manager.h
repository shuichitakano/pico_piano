/*
 * author : Shuichi TAKANO
 * since  : Sun Apr 09 2023 19:49:58
 */
#pragma once

#include <string>
#include <cstdint>
#include <btstack.h>
#include <vector>
#include <memory>

namespace bluetooth
{
    class Service;
    class Characteristic;
    class AdvertisingReport;

    ///////////////////////////////////////////
    class BLEClientHandler
    {
    public:
        virtual ~BLEClientHandler() = default;

        virtual bool onUpdateAdvertisingReport(const AdvertisingReport &ad) { return false; }
        virtual bool onEnumServiceCharacteristic(const Service &service, const Characteristic &chr) { return false; }

        virtual void onNotify(const uint8_t *p, size_t size, int handle) {}
        virtual void onRead(const uint8_t *p, size_t size, int handle) {}
        virtual void onWriteComplete(bool result, int handle) {}
        virtual void onDisconnect() {}

        void _set(hci_con_handle_t h, btstack_packet_handler_t handler);
        hci_con_handle_t _getConnectionHandle() const { return conHandle_; }
        btstack_packet_handler_t _getPacketHandler() const { return packetHandler_; }

    protected:
        bool write(int valueHandle, const uint8_t *data, size_t size, bool needResponse) const;

    private:
        hci_con_handle_t conHandle_{};
        btstack_packet_handler_t packetHandler_{};
    };

    ///////////////////////////////////////////
    using BDAddr = std::array<uint8_t, 6>;

    ///////////////////////////////////////////
    class Characteristic
    {
        gatt_client_characteristic_t obj_;
        std::string uuid_;

        gatt_client_notification_t notificationListener_;
        bool enabled_ = false;

    public:
        Characteristic(const gatt_client_characteristic_t &obj);

        int getStartHandle() const { return obj_.start_handle; }
        int getValueHandle() const { return obj_.value_handle; }
        int getEndHandle() const { return obj_.end_handle; }

        bool hasBroadcastProp() const { return obj_.properties & ATT_PROPERTY_BROADCAST; }
        bool hasReadProp() const { return obj_.properties & ATT_PROPERTY_READ; }
        bool hasWriteWithoutResponseProp() const { return obj_.properties & ATT_PROPERTY_WRITE_WITHOUT_RESPONSE; }
        bool hasWriteProp() const { return obj_.properties & ATT_PROPERTY_WRITE; }
        bool hasNotifyProp() const { return obj_.properties & ATT_PROPERTY_NOTIFY; }
        bool hasIndicateProp() const { return obj_.properties & ATT_PROPERTY_INDICATE; }
        bool hasSignedWriteProp() { return obj_.properties & ATT_PROPERTY_AUTHENTICATED_SIGNED_WRITE; }
        bool hasExtendedProperties() { return obj_.properties & ATT_PROPERTY_EXTENDED_PROPERTIES; }

        int getProperties() const { return obj_.properties; }

        const std::string &getUUIDString() const { return uuid_; }

        void _setEnable(bool f = true) { enabled_ = f; }
        bool _isEnabled() const { return enabled_; }
        bool _start(btstack_packet_handler_t handler, hci_con_handle_t conHandle);
    };

    ///////////////////////////////////////////
    class Service
    {
        gatt_client_service_t obj_;
        std::string uuid_;

        std::vector<Characteristic> characteristics_;

    public:
        Service(const gatt_client_service_t &obj);

        const gatt_client_service_t &getRaw() const { return obj_; }
        const std::string &getUUIDString() const { return uuid_; }

        size_t getCharacteristicsCount() const { return characteristics_.size(); }
        Characteristic &getCharacteristics(size_t i) { return characteristics_[i]; }
        Characteristic &getLatestCharacteristics() { return characteristics_.back(); }

        void addCharacteristics(Characteristic &&v) { characteristics_.push_back(std::move(v)); }
    };

    ///////////////////////////////////////////
    class AdvertisingReport
    {
        std::string name_;
        std::string uuid_;

        uint8_t type_;
        uint8_t eventType_;
        bd_addr_type_t addressType_;
        BDAddr address_;
        int8_t rssi_;

    public:
        void import(const uint8_t *packet);
        void dump() const;

        const std::string &getName() const { return name_; }
        const std::string &getUUIDString() const { return uuid_; }
        const BDAddr &getAddress() const { return address_; }
        bd_addr_type_t getAddressType() const { return addressType_; }
        int getRSSI() const { return rssi_; }
    };

    ///////////////////////////////////////////
    struct ClientEventHandlerInfo;

    ///////////////////////////////////////////
    ///////////////////////////////////////////
    class BLEClientManager
    {
    public:
        class Connection;

        BLEClientManager() = default;

        void registerHandler(BLEClientHandler *h);

        void onHCIEvent(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
        void onGATTEvent(Connection *conn, uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size);

        BLEClientManager(const BLEClientManager &) = delete;
        BLEClientManager &operator=(const BLEClientManager &) = delete;
        static BLEClientManager &instance();

    public:
        class Connection
        {
            BLEClientHandler *clientHandler_;
            ClientEventHandlerInfo *eventHandlerInfo_;
            hci_con_handle_t conHandle_;

            using Handler = bool (Connection::*)(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size);
            Handler currentHandler_{};

            std::vector<Service> services_;
            size_t discoveringService_ = 0;

        public:
            Connection(ClientEventHandlerInfo *hinfo, hci_con_handle_t conHandle, BLEClientHandler *h);
            ~Connection();

            void startSearchService();
            void startSearchCharacteristics(const Service &service);

            hci_con_handle_t getConnectionHandle() const { return conHandle_; }
            void onGATTEvent(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size);

        protected:
            void resetDiscoveringCharacteristicServiceIndex() { discoveringService_ = -1; }
            bool searchNextCharacteristics();
            void startValueUpdate();

            bool searchServiceHandler(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size);
            bool searchCharacteristicsHandler(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size);
            bool notifyHandler(uint8_t packet_type, uint16_t channel, const uint8_t *packet, uint16_t size);
            bool checkQueryCompleteStatus(const uint8_t *packet) const;
        };

    protected:
        struct HandlerState
        {
            BLEClientHandler *handler_;
            std::unique_ptr<Connection> connection_;
            BDAddr conAddress_{};

            void release();
            hci_con_handle_t getConnectionHandle() const;

        public:
            HandlerState(BLEClientHandler *h) : handler_(h) {}
        };

    private:
        std::vector<HandlerState> handlers_;
    };

    ///////////////////////////////////////////

    bool initializeBluetooth(bool client, bool server,
                             io_capability_t ioCap,
                             bool needSecureConnect,
                             bool needBonding,
                             bool needMITMProtection,
                             bool needkeyPress);

    void removeBondingInfoForDebug();
}