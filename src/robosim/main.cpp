#include <cmath>
#include <random>

#include <raylib.h>

#include "raylib/Color.hpp"
#include "raylib/raylib-cpp.hpp"
#include "rlImGui/rlImGui.h"
#include "imgui.h"

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
	int screenWidth = 1280;
	int screenHeight = 720;

	raylib::Window window(screenWidth, screenHeight, "raylib-cpp - basic window");
	SetTargetFPS(144);
	rlImGuiSetup(true);

	Bot bot{0, 0, {screenWidth / 2.0f, screenHeight / 2.0f}};

	while (!window.ShouldClose()) {
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

			EndDrawing();
		}
	}

	return 0;
}
