#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "network.h"
#include "arp_monitor.h"
#include "cable_tester.h"
#include "ecdsa_validator.h"
#include "exposure_checker.h"
#include "topology_scanner.h"

#include "app-window.h"

#ifdef __ANDROID__
extern "C" void slint_main()
#else
int main(int argc, char **argv)
#endif

{
    auto ui = OBMainScreen::create();
    slint::ComponentWeakHandle<OBMainScreen> weak_ui(ui);

    ohboyscan::ScanEngine engine;
    ohboyscan::ArpMonitor arp_monitor;

    auto arp_alerts = std::make_shared<slint::VectorModel<ArpAlert>>();
    ui->set_arp_alerts(arp_alerts);

    arp_monitor.set_event_callback([weak_ui, arp_alerts](const ohboyscan::ArpMonitor::Event& event) {
        slint::invoke_from_event_loop(
            [weak_ui, arp_alerts, message = event.message, alert = event.alert]() {
                if (auto ui = weak_ui.lock()) {
                    arp_alerts->push_back(ArpAlert{
                        slint::SharedString(message),
                        alert ? FeatureStatus::Error : FeatureStatus::Ok,
                    });
                    (*ui)->set_arp_status(slint::SharedString(
                        alert ? "ALERT: possible ARP spoofing" : "Watching gateway…"));
                }
            });
    });

    constexpr int kEcdsaFeature = 0;
    constexpr int kTopologyFeature = 1;
    constexpr int kArpFeature = 2;
    constexpr int kCableFeature = 3;
    constexpr int kExposureFeature = 4;

    ui->on_feature_opened([&](int index) {
        if (index == kEcdsaFeature) {
            ui->set_ecdsa_status(slint::SharedString("Auditing scripts…"));
            ui->set_ecdsa_results(std::make_shared<slint::VectorModel<EcdsaCheck>>());

            engine.submit([weak_ui]() {
                std::vector<EcdsaCheck> checks;

                const bool crypto_ok = ohboyscan::EcdsaValidator::self_test();
                checks.push_back(EcdsaCheck{
                    slint::SharedString("Crypto engine"),
                    slint::SharedString(crypto_ok ? "OK" : "FAILED"),
                    crypto_ok ? FeatureStatus::Ok : FeatureStatus::Error,
                });

                std::string rest_status = "Unreachable";
                FeatureStatus rest_kind = FeatureStatus::Error;

                ohboyscan::GatewayDetector detector;
                if (auto gateway = detector.detect_default_gateway()) {
                    ohboyscan::MikrotikDevice device(gateway->ip);
                    ohboyscan::RouterOsApiClient client(device);
                    const auto response = client.get("system/script");
                    if (response.status_code == 200) {
                        rest_status = "200 OK";
                        rest_kind = FeatureStatus::Ok;
                    } else if (response.status_code == 401 || response.status_code == 403) {
                        rest_status = std::to_string(response.status_code) + " auth failed";
                        rest_kind = FeatureStatus::Warn;
                    } else if (response.status_code > 0) {
                        rest_status = std::to_string(response.status_code);
                        rest_kind = FeatureStatus::Warn;
                    }
                }
                checks.push_back(EcdsaCheck{
                    slint::SharedString("REST API"),
                    slint::SharedString(rest_status),
                    rest_kind,
                });

                slint::invoke_from_event_loop([weak_ui, checks = std::move(checks)]() {
                    if (auto ui = weak_ui.lock()) {
                        auto model = std::make_shared<slint::VectorModel<EcdsaCheck>>(std::move(checks));
                        (*ui)->set_ecdsa_results(model);
                        (*ui)->set_ecdsa_status(slint::SharedString("Audit complete"));
                    }
                });
            });
            return;
        }

        if (index == kTopologyFeature) {
            ui->set_topology_status(slint::SharedString("Listening for neighbors…"));
            ui->set_topology_results(std::make_shared<slint::VectorModel<TopologyDevice>>());

            engine.submit([weak_ui]() {
                ohboyscan::GatewayDetector detector;
                auto gateway = detector.detect_default_gateway();
                const std::string interface = gateway ? gateway->interface : std::string{};

                ohboyscan::TopologyScanner scanner;
                auto result = scanner.scan(interface, std::chrono::milliseconds(4000));

                slint::invoke_from_event_loop([weak_ui, result = std::move(result)]() {
                    if (auto ui = weak_ui.lock()) {
                        if (!result.error.empty()) {
                            (*ui)->set_topology_status(slint::SharedString(result.error));
                            return;
                        }

                        std::vector<TopologyDevice> devices;
                        devices.reserve(result.devices.size());
                        for (const auto& device : result.devices) {
                            devices.push_back(TopologyDevice{
                                slint::SharedString(device.identity),
                                slint::SharedString(device.model),
                                slint::SharedString(device.version),
                                slint::SharedString(device.ip),
                                slint::SharedString(device.interface),
                                slint::SharedString(device.protocol),
                            });
                        }

                        auto model =
                            std::make_shared<slint::VectorModel<TopologyDevice>>(std::move(devices));
                        (*ui)->set_topology_results(model);
                        (*ui)->set_topology_status(
                            slint::SharedString(result.devices.empty() ? "No neighbors found"
                                                                       : "Scan complete"));
                    }
                });
            });
            return;
        }

        if (index == kArpFeature) {
            arp_monitor.stop();

            arp_alerts->clear();
            ui->set_arp_status(slint::SharedString("Starting monitor…"));

            ohboyscan::GatewayDetector detector;
            auto gateway = detector.detect_default_gateway();
            if (!gateway) {
                ui->set_arp_status(slint::SharedString("No default gateway found"));
                return;
            }

            if (!arp_monitor.start(gateway->interface, gateway->ip)) {
                ui->set_arp_status(slint::SharedString("Capture requires root or CAP_NET_RAW"));
                return;
            }

            ui->set_arp_status(
                slint::SharedString("Watching " + gateway->ip + " on " + gateway->interface));
            return;
        }

        if (index == kCableFeature) {
            ui->set_cable_status(slint::SharedString("Running cable test…"));
            ui->set_cable_results(std::make_shared<slint::VectorModel<CableResult>>());

            engine.submit([weak_ui]() {
                ohboyscan::GatewayDetector detector;
                auto gateway = detector.detect_default_gateway();

                std::vector<CableResult> results;
                if (!gateway) {
                    results.push_back(CableResult{
                        slint::SharedString("Gateway"),
                        slint::SharedString("not found"),
                        slint::SharedString(""),
                        FeatureStatus::Warn,
                    });
                } else {
                    ohboyscan::MikrotikDevice device(gateway->ip);
                    ohboyscan::CableTester tester;
                    const auto findings = tester.test(device);

                    results.reserve(findings.size());
                    for (const auto& finding : findings) {
                        FeatureStatus kind = FeatureStatus::Warn;
                        if (finding.status == "link-ok") {
                            kind = FeatureStatus::Ok;
                        } else if (finding.status == "open" || finding.status == "short") {
                            kind = FeatureStatus::Error;
                        }
                        results.push_back(CableResult{
                            slint::SharedString(finding.interface),
                            slint::SharedString(finding.status),
                            slint::SharedString(finding.distance),
                            kind,
                        });
                    }
                }

                slint::invoke_from_event_loop([weak_ui, results = std::move(results)]() {
                    if (auto ui = weak_ui.lock()) {
                        auto model =
                            std::make_shared<slint::VectorModel<CableResult>>(std::move(results));
                        (*ui)->set_cable_results(model);
                        (*ui)->set_cable_status(slint::SharedString("Cable test complete"));
                    }
                });
            });
            return;
        }

        if (index != kExposureFeature) {
            return;
        }

        ui->set_exposure_status(slint::SharedString("Scanning gateway admin ports…"));
        ui->set_exposure_results(std::make_shared<slint::VectorModel<PortResult>>());

        engine.submit([weak_ui]() {
            ohboyscan::GatewayDetector detector;
            auto gateway = detector.detect_default_gateway();
            if (!gateway) {
                slint::invoke_from_event_loop([weak_ui]() {
                    if (auto ui = weak_ui.lock()) {
                        (*ui)->set_exposure_status(slint::SharedString("No default gateway found"));
                    }
                });
                return;
            }

            ohboyscan::MikrotikDevice device(gateway->ip);
            ohboyscan::ExposureChecker checker;
            const auto findings = checker.check(device);

            // Build plain data on the worker thread; create and attach the
            // Slint model only on the main thread.
            std::vector<PortResult> results;
            results.reserve(findings.size());
            for (const auto& finding : findings) {
                FeatureStatus kind = FeatureStatus::Ok;
                if (finding.open) {
                    kind = (finding.port == 80 || finding.port == 23)
                               ? FeatureStatus::Error
                               : FeatureStatus::Warn;
                }
                results.push_back(PortResult{
                    slint::SharedString(finding.name),
                    finding.port,
                    slint::SharedString(finding.open ? "OPEN" : "CLOSED"),
                    kind,
                });
            }

            slint::invoke_from_event_loop([weak_ui, results = std::move(results)]() {
                if (auto ui = weak_ui.lock()) {
                    auto model =
                        std::make_shared<slint::VectorModel<PortResult>>(std::move(results));
                    (*ui)->set_exposure_results(model);
                    (*ui)->set_exposure_status(slint::SharedString("Scan complete"));
                }
            });
        });
    });

    ui->on_feature_closed([&]() {
        arp_monitor.stop();
    });

    {
        ohboyscan::GatewayDetector detector;
        if (auto gateway = detector.detect_default_gateway()) {
            const auto local_ip = detector.local_ipv4(gateway->interface);
            std::string status = gateway->interface + ": ";
            status += local_ip ? *local_ip : "no IP";
            ui->set_appbar_status(slint::SharedString(status));
            ui->set_appbar_link_active(true);
        } else {
            ui->set_appbar_status(slint::SharedString("No network interface"));
            ui->set_appbar_link_active(false);
        }
    }

    ui->run();

    return 0;
}
