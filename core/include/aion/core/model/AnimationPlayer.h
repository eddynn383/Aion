#pragma once

#include "aion/core/model/Model.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Animațiile pornite din interacțiuni (Modul interacțiune): fiecare animație are ceasul ei, mai multe rulează în
// paralel, iar starea (unde a ajuns) ține de animație, nu de blocul care a pornit-o. Poza se scrie în modelul Modului
// interacțiune, de la poza de bază a documentului; cu două animații pe același obiect, câștigă ultima pornită.
namespace aion::model {

struct PlayRequest {
    AnimationId animation;
    PlayDirection direction = PlayDirection::Forward;
    int repeat = 1; // 0 = la infinit
    std::string action; // blocul care o pornește (același bloc oprește o buclă infinită)
};

class AnimationPlayer {
public:
    // Blocurile unei declanșări de regulă. Cele cu Comută se decid împreună: dacă toate animațiile lor sunt deschise
    // (la sfârșit sau în drum spre el), se închid toate; altfel se deschid toate. O dată, într-un sens: o animație
    // ajunsă deja la capătul acela nu mai face nimic; repetările reiau fiecare tură de la capătul de pornire.
    void start(const Model& base, const std::vector<PlayRequest>& requests);
    // Avansează cu `dt`; scrie în `runtime` poza animațiilor care rulează (și ultima poză a celor care tocmai s-au
    // oprit). True dacă a scris ceva.
    bool update(Model& runtime, const Model& base, double dt);
    [[nodiscard]] bool active() const;
    [[nodiscard]] double position(AnimationId animation) const; // secunde în animație (0 dacă n-a rulat)
    [[nodiscard]] bool playing(AnimationId animation) const;
    // Culoarea animată a obiectului (și după oprire: rămâne unde a ajuns animația).
    [[nodiscard]] std::optional<Color> color(const Model& base, InstanceId instance) const;
    // Camera ultimei animații pornite care rulează și are chei de cameră.
    [[nodiscard]] std::optional<CameraKey> camera(const Model& base) const;
    void reset();

private:
    struct State {
        double position = 0.0;
        int direction = 0; // +1 înainte, −1 înapoi, 0 oprită
        int remaining = 0; // ture după cea curentă (−1 = la infinit)
        std::uint64_t order = 0; // a câta pornire (ultima câștigă)
        std::string action;
    };
    std::map<AnimationId, State> m_states;
    std::uint64_t m_counter = 0;
};

} // namespace aion::model
