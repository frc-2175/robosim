#include <cmath>
#include <random>

#include <raylib.h>
#include <box2d/box2d.h>

#include "raylib/Color.hpp"
#include "raylib/raylib-cpp.hpp"
#include "rlImGui/rlImGui.h"
#include "imgui.h"
#include "b2DrawRayLib/b2DrawRayLib.hpp"
#include "networktables.h"

std::random_device rd{};
std::mt19937 gen{rd()};
std::normal_distribution<float> d{0, 3};

struct Bot {
	float angle;
	float vel;
	Vector2 pos;

	float getAngle() {
		return angle + d(gen);
	}

	float getVel() {
		return vel + d(gen);
	}

	Vector2 getPos() {
		return {pos.x + d(gen), pos.y + d(gen)};
	}
};

void periodic() {
}

int main() {
	NTClient client = {0};
    NTError res = NTConnect(&client, "127.0.0.1");
    if (res.Error) {
        printf("%s (%d)\n", res.Error, res.ErrorCode);
        return 1;
    }

    NTDisconnect(&client);

	int screenWidth = 1280;
	int screenHeight = 720;

	raylib::Window window(screenWidth, screenHeight, "raylib-cpp - basic window");
	SetTargetFPS(60);
	rlImGuiSetup(true);

	Bot bot{0, 0, {screenWidth / 2.0f, screenHeight / 2.0f}};

	b2World world(b2Vec2(0, -10));

	b2DrawRayLib drawer{ 10.0f };
	drawer.SetFlags(
        b2Draw::e_shapeBit |
        b2Draw::e_jointBit |
        b2Draw::e_aabbBit |
        b2Draw::e_pairBit |
        b2Draw::e_centerOfMassBit
    );
	world.SetDebugDraw(&drawer);

	b2BodyDef groundBodyDef;
	groundBodyDef.position.Set(0.0f, -10.0f);
	b2Body* groundBody = world.CreateBody(&groundBodyDef);
	b2PolygonShape groundBox;
	groundBox.SetAsBox(50.0f, 10.0f);
	groundBody->CreateFixture(&groundBox, 0.0f);

	b2BodyDef bodyDef;
	bodyDef.type = b2_dynamicBody;
	bodyDef.position.Set(0.0f, 4.0f);
	b2Body* body = world.CreateBody(&bodyDef);
	b2PolygonShape dynamicBox;
	dynamicBox.SetAsBox(1.0f, 1.0f);
	b2FixtureDef fixtureDef;
	fixtureDef.shape = &dynamicBox;
	fixtureDef.density = 1.0f;
	fixtureDef.friction = 0.3f;
	body->CreateFixture(&fixtureDef);

	float timeStep = 1.0f / 60.0f;
	int32 velocityIterations = 6;
	int32 positionIterations = 2;

	while (!window.ShouldClose()) {
		world.Step(timeStep, velocityIterations, positionIterations);
		
		// lets try to drive straight, naively

		if (bot.getVel() < 2) {
			bot.vel += 0.06f;
		}

		if (bot.getPos().y > 360) {
			bot.angle -= 2.0f - abs(bot.vel / 2);
		} else {
			bot.angle += 2.0f - abs(bot.vel / 2);
		}

		if (IsKeyDown(KEY_LEFT)) bot.angle -= 2.0f - abs(bot.vel / 2);
		if (IsKeyDown(KEY_RIGHT)) bot.angle += 2.0f - abs(bot.vel / 2);

		if (IsKeyDown(KEY_UP)) bot.vel += 0.06f;
		if (IsKeyDown(KEY_DOWN)) bot.vel -= 0.06f;

		bot.vel *= 0.975;
		bot.pos.x += bot.vel * cos(DEG2RAD * bot.angle);
		bot.pos.y += bot.vel * sin(DEG2RAD * bot.angle);

		{
			BeginDrawing();
			window.ClearBackground(RAYWHITE);

			DrawCircleV(bot.pos, 20.0f, raylib::Color::Red());

			DrawLineEx(bot.pos, {bot.pos.x + (20 * cosf(DEG2RAD * bot.angle)), bot.pos.y + (20 * sinf(DEG2RAD * bot.angle))}, 3, BLACK);

			{
				rlImGuiBegin();

				ImGui::Text("Velocity: %f", bot.vel);
				ImGui::Text("Angle: %f", bot.angle);
				ImGui::Text("Position: (%f, %f)", bot.pos.x, bot.pos.y);
				float err = pow(bot.pos.y - 360, 2);
				ImGui::Text("Err: %f", err);

				static float maxErr;
				if (err > maxErr) {
					maxErr = err;
				}

				ImGui::Text("Max Err: %f", maxErr);

				rlImGuiEnd();
			}

			world.DebugDraw();

			EndDrawing();
		}
	}

	return 0;
}
