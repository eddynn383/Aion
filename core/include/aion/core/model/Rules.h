#pragma once

#include "aion/core/model/Model.h"

#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// Executorul interacțiunilor (Modul interacțiune): la un eveniment pe un obiect pornește regulile lui, apoi
// avansează acțiunile în timp (mutări, rotiri, scalări animate; așteptări), una după alta. Lucrează pe modelul
// primit (în aplicație, o copie: Modul interacțiune nu atinge documentul).
namespace aion::model {

// Ce trebuie să facă aplicația (nu ține de geometrie): pornește o animație din timeline sau trece la o scenă.
struct HostRequest {
    AnimationId playAnimation;
    SceneId goToScene;
};

class RuleRuntime {
public:
    // Pornește regulile instanței pentru eveniment. Întoarce true dacă exista măcar o regulă potrivită.
    bool fire(Model& model, InstanceId self, Trigger trigger, std::string_view key = {});
    // Avansează cu dt secunde; întoarce true cât timp mai rulează ceva.
    bool update(Model& model, double dt);
    [[nodiscard]] bool active() const { return !m_running.empty(); }
    [[nodiscard]] std::vector<HostRequest> takeRequests() { return std::exchange(m_requests, {}); }
    void reset();

private:
    struct Queued {
        Action action;
        std::string path; // identifică blocul (pentru „Alternează”)
    };
    struct Step {
        Action action;
        InstanceId target;
        math::Transform from;
        math::Transform to;
        double elapsed = 0.0;
        double duration = 0.0;
        bool animated = false;
    };
    struct Sequence {
        InstanceId self;
        std::deque<Queued> queue;
        std::optional<Step> current;
    };

    // Începe următoarea acțiune din secvență (cele instantanee se aplică pe loc). False dacă secvența s-a terminat.
    bool startNext(Model& model, Sequence& sequence);

    std::vector<Sequence> m_running;
    std::map<std::pair<InstanceId, std::string>, int> m_alternate;
    std::vector<HostRequest> m_requests;
};

} // namespace aion::model
