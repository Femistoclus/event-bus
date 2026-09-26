#include <iostream>
#include <thread>

#include "event_bus/event_bus.hpp"

struct OrderEvent {
    uint64_t order_id;
    double price;
    bool is_buy;
};

int main() {
    event_bus::SyncEventBus<OrderEvent> sync_bus;
    sync_bus.subscribe<OrderEvent>([](const OrderEvent& e) {
        std::cout << "[sync] order " << e.order_id
                  << (e.is_buy ? " BUY " : " SELL ") << e.price << "\n";
    });
    sync_bus.seal();
    sync_bus.publish(OrderEvent{1, 100.5, true});

    event_bus::AsyncEventBus<64, OrderEvent> async_bus;
    async_bus.subscribe<OrderEvent>([](const OrderEvent& e) {
        std::cout << "[async] order " << e.order_id
                  << (e.is_buy ? " BUY " : " SELL ") << e.price
                  << " (dispatched on main thread)\n";
    });
    async_bus.seal();

    std::thread producer([&async_bus] {
        for (uint64_t id = 2; id <= 4; ++id) {
            [[maybe_unused]] bool enqueued =
                async_bus.publish(OrderEvent{id, 100.0 + id, id % 2 == 0});
        }
    });
    producer.join();

    async_bus.dispatch();

    return 0;
}
