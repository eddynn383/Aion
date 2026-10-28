#pragma once

#include "aion/core/model/Model.h"

#include <optional>

// Căile de parcurgere (ADR 0036): obiectele ancorate merg pe curbă cu fața înainte, cu baza (centrul feței de jos a
// cutiei lor) pe cale; decalajul fiecăruia (înainte, lateral, sus, rotire) e pe axele căii, la locul lui. Toate
// funcțiile care schimbă modelul se apelează într-o comandă (un pas de undo).
namespace aion::model {

// Transformarea obiectului ancorat la `time` (secunde în animația căii), cu scara dată.
[[nodiscard]] math::Transform anchorTransform(const Model& model, const MotionPath& path, const PathAnchor& anchor,
    double time, const math::Vec3d& scale);
// Pune obiectele ancorate pe căile animației în poza `posed` (o copie a lui `source`), la `time`. Poziția și rotirea vin
// de la cale; scara din pista obiectului, dacă o animă, altfel cea de dinainte de ancorare.
void applyPaths(Model& posed, const Model& source, AnimationId animation, double time);

// Ancorează obiectul (de la rădăcina modelului): sare la startul căii, cu fața spre direcția ei (cât mai puțină rotire);
// în coloană, în urma ultimului ancorat. false dacă e deja pe o cale a aceleiași animații.
bool attachToPath(Model& model, MotionPathId path, InstanceId instance);
// Scoate obiectul de pe cale: revine exact la transformarea de dinainte de ancorare.
void detachFromPath(Model& model, MotionPathId path, InstanceId instance);
void removePath(Model& model, MotionPathId path); // scoate toate ancorele, apoi calea
void removeAnimationWithPaths(Model& model, AnimationId animation);
// Calea schimbată (curba, transformarea): obiectele ancorate se reașază la startul ei.
void updatePath(Model& model, MotionPathId id, MotionPath path);
// O mutare / rotire a obiectului ancorat (`edited`, la `time`) devine decalajul lui față de cale.
void absorbAnchorEdit(Model& model, MotionPathId path, InstanceId instance, const math::Transform& edited, double time);
void rotateAnchorFront(Model& model, MotionPathId path, InstanceId instance); // fața: următorul sfert de tură
[[nodiscard]] std::optional<MotionPathId> pathOf(const Model& model, AnimationId animation, InstanceId instance);

} // namespace aion::model
