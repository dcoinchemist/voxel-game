#pragma once
#include <cmath>
#include "engine/math.h"
#include "engine/world.h"

struct PlayerInput {
    float moveX = 0.0f;  // strafe: -1 left .. +1 right
    float moveY = 0.0f;  // forward: -1 back .. +1 forward
    bool jump = false;   // true while the jump button is held
};

class Player {
public:
    Vec3 pos = {0.0f, 0.0f, 0.0f};  // feet, centre of the bottom of the box
    Vec3 vel = {0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;    // degrees, 0 looks toward -Z
    float pitch = 0.0f;  // degrees, positive looks up
    bool onGround = false;

    static constexpr float RADIUS = 0.3f;
    static constexpr float HEIGHT = 1.8f;
    static constexpr float EYE_HEIGHT = 1.62f;
    static constexpr float SPEED = 4.5f;
    static constexpr float GRAVITY = 28.0f;
    static constexpr float JUMP_SPEED = 8.4f;
    static constexpr float MAX_FALL = 40.0f;
    static constexpr float STEP = 1.0f / 120.0f;  // fixed physics step

    void spawn(const World& world, float x, float z);
    void look(float dYawDeg, float dPitchDeg);
    void update(const World& world, float dt, const PlayerInput& in);
    Vec3 eyePosition() const { return {pos.x, pos.y + EYE_HEIGHT, pos.z}; }
    Vec3 forwardDir() const;

private:
    float accumulator_ = 0.0f;
    bool collides(const World& world, Vec3 p) const;
    void step(const World& world, const PlayerInput& in);
    void moveAxis(const World& world, int axis, float delta, bool allowStepUp);
};

inline void Player::spawn(const World& world, float x, float z) {
    int ix = (int)x;
    int iz = (int)z;
    int top = 0;
    for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
        if (BlockRegistry::isSolid(world.getBlock(ix, y, iz))) {
            top = y + 1;
            break;
        }
    }
    pos = {x, (float)top, z};
    vel = {0.0f, 0.0f, 0.0f};
    onGround = false;
}

inline void Player::look(float dYawDeg, float dPitchDeg) {
    yaw = std::fmod(yaw + dYawDeg, 360.0f);
    if (yaw < 0.0f) yaw += 360.0f;
    pitch += dPitchDeg;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

inline Vec3 Player::forwardDir() const {
    const float d2r = 3.14159265f / 180.0f;
    float yr = yaw * d2r;
    float pr = pitch * d2r;
    return {std::cos(pr) * std::sin(yr), std::sin(pr), -std::cos(pr) * std::cos(yr)};
}

inline bool Player::collides(const World& world, Vec3 p) const {
    const float eps = 0.0001f;
    int x0 = (int)std::floor(p.x - RADIUS);
    int x1 = (int)std::floor(p.x + RADIUS - eps);
    int y0 = (int)std::floor(p.y);
    int y1 = (int)std::floor(p.y + HEIGHT - eps);
    int z0 = (int)std::floor(p.z - RADIUS);
    int z1 = (int)std::floor(p.z + RADIUS - eps);
    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                if (BlockRegistry::isSolid(world.getBlock(x, y, z))) return true;
            }
        }
    }
    return false;
}

inline void Player::moveAxis(const World& world, int axis, float delta, bool allowStepUp) {
    if (delta == 0.0f) return;
    Vec3 p = pos;
    if (axis == 0) p.x += delta;
    else if (axis == 1) p.y += delta;
    else p.z += delta;

    if (!collides(world, p)) {
        pos = p;
        return;
    }
    if (axis == 1) {
        if (delta < 0.0f) {
            pos.y = std::floor(p.y) + 1.0f;  // land exactly on top of the block
            onGround = true;
        }
        vel.y = 0.0f;
        return;
    }
    // Blocked sideways: climb a one-block step if there is room.
    if (allowStepUp) {
        Vec3 up = pos;
        up.y += 1.0f;
        if (!collides(world, up)) {
            Vec3 q = up;
            if (axis == 0) q.x += delta;
            else q.z += delta;
            if (!collides(world, q)) {
                pos = q;
                return;
            }
        }
    }
    if (axis == 0) vel.x = 0.0f;
    else vel.z = 0.0f;
}

inline void Player::step(const World& world, const PlayerInput& in) {
    const float dt = STEP;
    bool wasOnGround = onGround;

    float yr = yaw * 3.14159265f / 180.0f;
    float fx = std::sin(yr), fz = -std::cos(yr);  // forward
    float rx = std::cos(yr), rz = std::sin(yr);   // right
    float mx = in.moveX;
    float my = in.moveY;
    float len = std::sqrt(mx * mx + my * my);
    if (len > 1.0f) {
        mx /= len;
        my /= len;
    }
    vel.x = (rx * mx + fx * my) * SPEED;
    vel.z = (rz * mx + fz * my) * SPEED;

    vel.y -= GRAVITY * dt;
    if (vel.y < -MAX_FALL) vel.y = -MAX_FALL;
    if (in.jump && wasOnGround) vel.y = JUMP_SPEED;

    onGround = false;
    moveAxis(world, 0, vel.x * dt, wasOnGround);
    moveAxis(world, 2, vel.z * dt, wasOnGround);
    moveAxis(world, 1, vel.y * dt, false);

    // Invisible walls around the edge of the test world.
    if (pos.x < RADIUS) pos.x = RADIUS;
    if (pos.z < RADIUS) pos.z = RADIUS;
    if (pos.x > WORLD_SIZE - RADIUS) pos.x = WORLD_SIZE - RADIUS;
    if (pos.z > WORLD_SIZE - RADIUS) pos.z = WORLD_SIZE - RADIUS;
}

inline void Player::update(const World& world, float dt, const PlayerInput& in) {
    if (dt > 0.1f) dt = 0.1f;
    accumulator_ += dt;
    while (accumulator_ >= STEP) {
        step(world, in);
        accumulator_ -= STEP;
    }
}
