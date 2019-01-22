// Game.cpp

#include "Main.hpp"
#include "Game.hpp"
#include "Engine.hpp"
#include "Renderer.hpp"
#include "AI.hpp"

bool intersectRayLine(const Vector& rayOrigin, const float rayAngle, const Vector& lineStart, const Vector& lineEnd, Vector& out) {
	Vector r1 = rayOrigin;
	r1.x += cosf(rayAngle);
	r1.y += sinf(rayAngle);

	Vector l = Vector(lineStart.x, lineStart.y, 1.f).cross(Vector(lineEnd.x, lineEnd.y, 1.f));
	Vector m = Vector(rayOrigin.x, rayOrigin.y, 1.f).cross(Vector(r1.x, r1.y, 1.f));
	Vector i = l.cross(m);

	if (i.z == 0.f) {
		out = rayOrigin;
		return true;
	} else {
		Vector q(i.x / i.z, i.y / i.z, 0.f);
		Vector d(lineEnd - lineStart);
		Vector o(q - lineStart);
		if (d.lengthSquared() < o.lengthSquared() || d.dot(o) < 0.f || (q - rayOrigin).dot(r1 - rayOrigin) < 0.f) {
			return false;
		} else {
			out = q;
			return true;
		}
	}
}

int intersectRayCircle(const Vector& rayOrigin, const float rayAngle, const Vector& circleOrigin, const float radius, Vector& out1, Vector& out2) {
	Vector r1;
	r1.x += cosf(rayAngle);
	r1.y += sinf(rayAngle);

	Vector ac = circleOrigin - rayOrigin;
	float ac1 = ac.dot(r1);
	Vector ra = r1 * ac1;
	Vector r = ra + rayOrigin;

	if (ra.dot(r1) < 0.f) {
		return 0;
	}

	float dist = (r - circleOrigin).length();
	float a = asinf(dist / radius);
	float c = cosf(a) * radius;

	if (dist == radius) {
		out1 = r;
		out2 = r;
		return 1;
	} else if (dist < radius) {
		out1 = r - r1 * c;
		out2 = r + r1 * c;
		return 2;
	} else {
		return 0;
	}
}

Game::Game(float _boardW, float _boardH) {
	boardW = _boardW;
	boardH = _boardH;
	ai = new AI(this);
	ai->init((int)boardW, (int)boardH);
	if (!ai) {
		init();
	}
	ticksPerSecond = mainEngine->getTicksPerSecond();
}

Game::~Game() {
	term();
	if (ai) {
		delete ai;
		ai = nullptr;
	}
}

int Game::playSound(const char* filename, bool loop) {
	if (!ai) {
		return mainEngine->playSound(filename, loop);
	} else {
		return 0;
	}
}

int Game::stopSound(int channel) {
	if (!ai) {
		return mainEngine->stopSound(channel);
	} else {
		return 0;
	}
}

void Game::init() {
	rand.seedValue(0);
	spawnPlayer();
	spawnAsteroids();
	gameInSession = true;
}

int Game::countAsteroids() {
	int result = 0;
	for (auto entity : entities) {
		if (entity->team != Entity::Team::TEAM_ALLY) {
			++result;
		}
	}
	return result;
}

void Game::spawnPlayer() {
	player = new Player(this);
	player->ang = 3.f * PI / 2.f;
	player->radius = 10.f;
	player->team = Entity::Team::TEAM_ALLY;
	entities.addNodeLast(player);
}

void Game::spawnAsteroids() {
	Uint8 asteroidNum = 10;
	for (Uint8 c = 0; c < asteroidNum; ++c) {
		Asteroid* asteroid = new Asteroid(this);
		asteroid->radius = 40.f;
		asteroid->vel = Vector(rand.getFloat(), rand.getFloat(), 0.f);
		asteroid->pos = Vector((rand.getFloat() - 0.5f) * boardW, (rand.getFloat() - 0.5f) * boardH, 0.f);
		if ((asteroid->pos - player->pos).length() < 200.f) {
			asteroid->pos.x += boardW / 2.f;
			asteroid->pos.y += boardH / 2.f;
		}
		asteroid->team = Entity::Team::TEAM_ENEMY;
		entities.addNodeLast(asteroid);
	}
	beat = 70;
	previousBeat = false;
}

void Game::term() {
	player = nullptr;
	for (auto& entity : entities) {
		delete entity;
	}
	entities.removeAll();

	lossTimer = -1.f;
	wonTimer = 0.f;
	wins = 0;
	losses = 0;
	score = 0;
	lives = 3;
	beat = 70;
	previousBeat = false;
	ticks = 0;
	gameInSession = false;
}

void Game::draw(Camera& camera) {
	Renderer* renderer = mainEngine->getRenderer();
	assert(renderer);

	for (auto& entity : entities) {
		entity->draw(camera);
	}

	// score
	{
		Rect<int> rect;
		rect.x = 10; rect.y = 10;
		rect.w = 0; rect.h = 0;
		StringBuf<32> buf("Score: %d", score);
		renderer->printText(rect, buf.get());
	}

	// lives
	{
		Rect<int> rect;
		rect.x = 10; rect.y = 30;
		rect.w = 0; rect.h = 0;
		StringBuf<32> buf("Lives: %d", lives);
		renderer->printText(rect, buf.get());
	}

	// ai stats
	if (ai) {
		// generation
		{
			Rect<int> rect;
			rect.x = 10; rect.y = 50;
			rect.w = 0; rect.h = 0;
			StringBuf<32> buf("Generation: %d", ai->getGeneration());
			renderer->printText(rect, buf.get());
		}

		// species
		{
			Rect<int> rect;
			rect.x = 10; rect.y = 70;
			rect.w = 0; rect.h = 0;
			StringBuf<32> buf("Species: %d", ai->getSpecies());
			renderer->printText(rect, buf.get());
		}

		// genome
		{
			Rect<int> rect;
			rect.x = 10; rect.y = 90;
			rect.w = 0; rect.h = 0;
			StringBuf<32> buf("Genome: %d", ai->getGenome());
			renderer->printText(rect, buf.get());
		}

		// max fitness
		{
			Rect<int> rect;
			rect.x = 10; rect.y = 110;
			rect.w = 0; rect.h = 0;
			StringBuf<32> buf("Max fitness: %d", ai->getMaxFitness());
			renderer->printText(rect, buf.get());
		}
	}
}

void Game::doKeyboardInput() {
	inputs[IN_THRUST] = mainEngine->getKeyStatus(SDL_SCANCODE_UP);
	inputs[IN_RIGHT] = mainEngine->getKeyStatus(SDL_SCANCODE_RIGHT);
	inputs[IN_LEFT] = mainEngine->getKeyStatus(SDL_SCANCODE_LEFT);
	inputs[IN_SHOOT] = mainEngine->getKeyStatus(SDL_SCANCODE_SPACE);
}

void Game::doAI() {
	if (mainEngine->pressKey(SDL_SCANCODE_F1)) {
		ai->save();
	}
	if (mainEngine->pressKey(SDL_SCANCODE_F2)) {
		ai->load();
	}
	if (mainEngine->pressKey(SDL_SCANCODE_F3)) {
		ai->playTop();
	}

	// step AI
	ai->process();

	// apply inputs
	inputs[IN_THRUST] = ai->outputs[AI::Output::OUT_THRUST];
	inputs[IN_RIGHT] = ai->outputs[AI::Output::OUT_RIGHT];
	inputs[IN_LEFT] = ai->outputs[AI::Output::OUT_LEFT];
	inputs[IN_SHOOT] = ai->outputs[AI::Output::OUT_SHOOT];
}

void Game::process() {
	if (!gameInSession) {
		return;
	}
	if (ai) {
		doAI();
	} else {
		doKeyboardInput();
	}

	// process entities
	Node<Entity*>* nextnode = nullptr;
	for (Node<Entity*>* node = entities.getFirst(); node != nullptr; node = nextnode) {
		nextnode = node->getNext();
		Entity* entity = node->getData();

		entity->process();

		// wrap position
		entity->pos.x += boardW / 2.f;
		entity->pos.x = fmod(entity->pos.x, boardW);
		if (entity->pos.x < 0.f) {
			entity->pos.x += boardW;
		}
		entity->pos.x -= boardW / 2.f;

		entity->pos.y += boardH / 2.f;
		entity->pos.y = fmod(entity->pos.y, boardH);
		if (entity->pos.y < 0.f) {
			entity->pos.y += boardH;
		}
		entity->pos.y -= boardH / 2.f;

		entity->pos.z = 0.f;
		entity->ang = fmod(entity->ang, PI * 2.f);

		// do collisions
		for (Node<Entity*>* otherNode = nextnode; otherNode != nullptr; otherNode = otherNode->getNext()) {
			if (entity->dead) {
				break;
			}
			Entity* other = otherNode->getData();
			if (entity->lastEntityHit == other || other->dead) {
				continue;
			}

			float dist = (entity->pos - other->pos).lengthSquared();
			float radii = (entity->radius + other->radius) * (entity->radius + other->radius);
			if (dist <= radii) {
				entity->onHit(other);
				other->onHit(entity);
			}
		}

		// remove dead entities
		if (entity->dead) {
			if (player == entity) {
				player = nullptr;
				lossTimer = 0.f;
			}
			delete entity;
			entities.removeNode(node);
		}
	}

	// end round timer
	int numAsteroids = countAsteroids();
	if (numAsteroids == 0) {
		wonTimer += 1.f;
		if (wonTimer > 120.f) {
			++wins;
			wonTimer = 0.f;
			spawnAsteroids();
			score += 1000;
			player->ticks = 0;
		}
	}
	if (player == nullptr && lossTimer >= 0.f) {
		lossTimer += 1.f;
		if (lossTimer > 150.f) {
			++losses;
			lossTimer = -1.f;
			if (lives > 0) {
				spawnPlayer();
				--lives;
			}
		}
	}

	// spawn aliens
	if (numAsteroids > 0 && ticks && ticks % (15 * ticksPerSecond) == 0 && rand.getUint8() % 2 == 0) {
		Alien* alien = new Alien(this);
		bool right = rand.getUint8() % 2 == 0;
		alien->pos.x = right ? boardW / 2.f : -boardW / 2.f;
		alien->pos.y = rand.getFloat() * boardH - boardH / 2.f;
		alien->ang = right ? 0.f : PI;
		alien->vel = right ? Vector(2.f, 0.f, 0.f) : Vector(-2.f, 0.f, 0.f);
		alien->life = boardW;
		alien->team = Entity::Team::TEAM_ENEMY;
		alien->radius = 20.f;
		alien->channel = playSound("sounds/alien.wav", true);
		addEntity(alien);
	}

	// beat
	if (beat < 20) {
		beat = 20;
	}
	if (ticks && ticks % beat == 0) {
		previousBeat = (previousBeat == false);
		const char* path = previousBeat ? "sounds/beat.wav" : "sounds/beat2.wav";
		playSound(path, false);
	}

	++ticks;
}

void Game::addEntity(Entity* entity) {
	entities.addNodeLast(entity);
}

void Entity::process() {
	pos += vel;
	++ticks;
}

bool Entity::onHit(const Entity* other) {
	if (!other) {
		return false;
	}
	if (other->team != team) {
		if (team == Entity::Team::TEAM_NONE || other->team == Entity::Team::TEAM_NONE) {
			return false;
		}
		if ((game->player != this || this->ticks >= Player::shieldTime) &&
			(game->player != other || other->ticks >= Player::shieldTime)) {
			if (team == Team::TEAM_ENEMY && other->getType() == Type::TYPE_BULLET) {
				auto bullet = static_cast<const Bullet*>(other);
				if (bullet->parent) {
					++bullet->parent->shotsHit;
				}
				game->score += getPoints();
				game->beat -= 2;
			}
			dead = true;
			return true;
		} else {
			return false;
		}
	}
	lastEntityHit = other;
	return false;
}

void Entity::shootBullet(float speed, float range) {
	Bullet* bullet = new Bullet(game);
	Vector heading(cosf(ang), sinf(ang), 0.f);
	bullet->pos = pos + heading * radius;
	bullet->vel = vel + heading * speed;
	bullet->life = range;
	bullet->team = team;
	bullet->radius = 2.f;
	bullet->parent = this;
	game->addEntity(bullet);
	++shotsFired;
}

float Entity::rayTrace(Vector origin, float angle, int disableSide, int count) {
	float result = FLT_MAX;
	Vector intersect;

	// test against entities
	for (auto entity : game->entities) {
		if (entity == this || entity->team == Team::TEAM_NONE || entity->team == team) {
			continue;
		}
		Vector intersect1, intersect2;
		int num = intersectRayCircle(origin, angle, entity->pos, entity->radius, intersect1, intersect2);
		if (num) {
			result = std::min(result, (intersect1 - origin).length());
		}
	}

	// test against edge of board
	if (result == FLT_MAX && count < 2) {
		if (disableSide != 1) { // top
			Vector start(-game->boardW / 2.f, -game->boardH / 2.f, 0.f);
			Vector   end(game->boardW / 2.f, -game->boardH / 2.f, 0.f);
			if (intersectRayLine(origin, angle, start, end, intersect)) {
				result = rayTrace(Vector(intersect.x, -intersect.y, 0.f), angle, 2, count + 1);
				if (result != FLT_MAX) {
					float distToEdge = (intersect - origin).length();
					return distToEdge + result;
				}
			}
		}
		if (disableSide != 2) { // bottom
			Vector start(-game->boardW / 2.f, game->boardH / 2.f, 0.f);
			Vector   end(game->boardW / 2.f, game->boardH / 2.f, 0.f);
			if (intersectRayLine(origin, angle, start, end, intersect)) {
				result = rayTrace(Vector(intersect.x, -intersect.y, 0.f), angle, 1, count + 1);
				if (result != FLT_MAX) {
					float distToEdge = (intersect - origin).length();
					return distToEdge + result;
				}
			}
		}
		if (disableSide != 3) { // left
			Vector start(-game->boardW / 2.f, -game->boardH / 2.f, 0.f);
			Vector   end(-game->boardW / 2.f, game->boardH / 2.f, 0.f);
			if (intersectRayLine(origin, angle, start, end, intersect)) {
				result = rayTrace(Vector(-intersect.x, intersect.y, 0.f), angle, 4, count + 1);
				if (result != FLT_MAX) {
					float distToEdge = (intersect - origin).length();
					return distToEdge + result;
				}
			}
		}
		if (disableSide != 4) { // right
			Vector start(game->boardW / 2.f, -game->boardH / 2.f, 0.f);
			Vector   end(game->boardW / 2.f, game->boardH / 2.f, 0.f);
			if (intersectRayLine(origin, angle, start, end, intersect)) {
				result = rayTrace(Vector(-intersect.x, intersect.y, 0.f), angle, 3, count + 1);
				if (result != FLT_MAX) {
					float distToEdge = (intersect - origin).length();
					return distToEdge + result;
				}
			}
		}
	}

	return result;
}

const float Player::shieldTime = 0.f;

void Player::process() {
	Vector front(cosf(ang), sinf(ang), 0.f);

	int bullets = 0;
	for (auto entity : game->entities) {
		if (entity->team == Team::TEAM_ALLY && entity != game->player) {
			++bullets;
		}
	}
	if (bullets == 0 && vel.lengthSquared() == 0.f) {
		moved = false;
	}

	if (game->inputs[Game::Input::IN_RIGHT]) {
		ang += PI / game->ticksPerSecond;
		moved = true;
	}
	if (game->inputs[Game::Input::IN_LEFT]) {
		ang -= PI / game->ticksPerSecond;
		moved = true;
	}
	if (game->inputs[Game::Input::IN_THRUST]) {
		vel += (front * 10.f) / game->ticksPerSecond;
		moved = true;
	}
	if (game->inputs[Game::Input::IN_SHOOT]) {
		if (!shooting && ticks - shootTime > 6) {
			shootTime = ticks;
			shootBullet(10.f, 40.f);
			game->playSound("sounds/shoot.wav", false);
			moved = true;
			shooting = true;
		}
	}
	if (!game->inputs[Game::Input::IN_SHOOT]) {
		shooting = false;
	}
	if (vel.lengthSquared() > 100.f) {
		vel = vel.normal() * 10.f;
	}

	Entity::process();
}

void Player::draw(Camera& camera) {
	static const WideVector color = WideVector(0.f, 1.f, 0.f, 1.f);

	Vector src, dest;
	Vector front(cosf(ang), sinf(ang), 0.f);
	Vector right(cosf(ang + PI/2.f), sinf(ang + PI/2.f), 0.f);

	src = pos + front * 10.f;
	dest = pos - front * 10.f + right * 10.f;
	camera.line->drawLine(camera, src, dest, color);

	src = pos + front * 10.f;
	dest = pos - front * 10.f - right * 10.f;
	camera.line->drawLine(camera, src, dest, color);

	// draw shield
	if (ticks < shieldTime) {
		float angle = 0.f;
		static const int steps = 15;
		static const float finc = (PI * 2.f) / (float)steps;
		for (int c = 0; c < steps; ++c) {
			Vector src = pos;
			Vector dest = pos;

			src.x += cosf(angle) * radius * 2.f;
			src.y += sinf(angle) * radius * 2.f;
			dest.x += cosf(angle + finc) * radius * 2.f;
			dest.y += sinf(angle + finc) * radius * 2.f;

			camera.line->drawLine(camera, src, dest, color);
			angle += finc;
		}
	}
}

bool Player::onHit(const Entity* other) {
	if (Entity::onHit(other)) {
		Explosion* explosion = new Explosion(game);
		explosion->pos = pos;
		explosion->radius = 0.f;
		game->addEntity(explosion);
		game->playSound("sounds/die.wav", false);
		game->beat -= 10;
		return true;
	}
	return false;
}

void Asteroid::process() {
	Entity::process();
}

void Asteroid::draw(Camera& camera) {
	float angle = 0.f;
	static const int steps = 15;
	static const float finc = (PI * 2.f) / (float)steps;
	for (int c = 0; c < steps; ++c) {
		Vector src = pos;
		Vector dest = pos;

		src.x += cosf(angle) * radius;
		src.y += sinf(angle) * radius;
		dest.x += cosf(angle + finc) * radius;
		dest.y += sinf(angle + finc) * radius;

		camera.line->drawLine(camera, src, dest, WideVector(1.f, 1.f, 1.f, 1.f));
		angle += finc;
	}
}

bool Asteroid::onHit(const Entity* other) {
	if (Entity::onHit(other)) {
		if (radius > 10.f && other->team != team) {
			auto& rand = game->rand;
			float ang = rand.getFloat() * PI * 2.f;
			float cang = cosf(ang);
			float sang = sinf(ang);

			for (int c = 0; c < 2; ++c) {
				Asteroid* asteroid = new Asteroid(game);
				asteroid->radius = radius / 2.f;
				asteroid->vel = vel + Vector(cang * rand.getFloat() * 5.f, sang * rand.getFloat() * 5.f, 0.f);
				asteroid->pos = pos + Vector(cang * radius / 2.f, sang * radius / 2.f, 0.f);
				asteroid->team = Entity::Team::TEAM_ENEMY;
				game->addEntity(asteroid);

				ang += PI;
				cang = cosf(ang);
				sang = sinf(ang);
			}
		}
		game->playSound("sounds/asteroid.wav", false);
		return true;
	}
	return false;
}

Alien::~Alien() {
	if (channel >= 0) {
		game->stopSound(channel);
	}
}

void Alien::process() {
	Entity::process();

	// shoot
	Player* player = game->player;
	if (player && ticks % game->ticksPerSecond == 0) {
		Vector diff = player->pos - pos;
		ang = atan2f(diff.y, diff.x);
		shootBullet(10.f, 40.f);
	}

	// state machine
	if (state == State::GO_STRAIGHT) {
		if (ticks % game->ticksPerSecond * 2 == 0 && game->rand.getUint8() % 3 == 0) {
			state = State::GO_DIAGONAL;
			vel.y = game->rand.getUint8() % 2 == 0 ? 2.f : -2.f;
		}
	} else {
		if (ticks % game->ticksPerSecond == 0) {
			state = State::GO_STRAIGHT;
			vel.y = 0.f;
		}
	}

	// die eventually
	life -= 1.f;
	if (life <= 0.f) {
		dead = true;
	}
}

bool Alien::onHit(const Entity* other) {
	if (Entity::onHit(other)) {
		Explosion* explosion = new Explosion(game);
		explosion->pos = pos;
		explosion->radius = 0.f;
		game->addEntity(explosion);
		game->playSound("sounds/aliendie.wav", false);
		return true;
	}
	return false;
}

void Alien::draw(Camera& camera) {
	Vector src, dest;
	static const WideVector color(1.f, 0.f, 0.f, 1.f);

	src = pos + Vector(-20.f, 0.f, 0.f);
	dest = pos + Vector(20.f, 0.f, 0.f);
	camera.line->drawLine(camera, src, dest, color);

	src = pos + Vector(-10.f, 8.f, 0.f);
	dest = pos + Vector(10.f, 8.f, 0.f);
	camera.line->drawLine(camera, src, dest, color);

	src = pos + Vector(-10.f, -8.f, 0.f);
	dest = pos + Vector(10.f, -8.f, 0.f);
	camera.line->drawLine(camera, src, dest, color);

	src = pos + Vector(-20.f, 0.f, 0.f);
	dest = pos + Vector(-10.f, -8.f, 0.f);
	camera.line->drawLine(camera, src, dest, color);

	src = pos + Vector(20.f, 0.f, 0.f);
	dest = pos + Vector(10.f, -8.f, 0.f);
	camera.line->drawLine(camera, src, dest, color);

	src = pos + Vector(-20.f, 0.f, 0.f);
	dest = pos + Vector(-10.f, 8.f, 0.f);
	camera.line->drawLine(camera, src, dest, color);

	src = pos + Vector(20.f, 0.f, 0.f);
	dest = pos + Vector(10.f, 8.f, 0.f);
	camera.line->drawLine(camera, src, dest, color);

	src = pos + Vector(-8.f, -15.f, 0.f);
	dest = pos + Vector(8.f, -15.f, 0.f);
	camera.line->drawLine(camera, src, dest, color);

	src = pos + Vector(-10.f, -8.f, 0.f);
	dest = pos + Vector(-8.f, -15.f, 0.f);
	camera.line->drawLine(camera, src, dest, color);

	src = pos + Vector(10.f, -8.f, 0.f);
	dest = pos + Vector(8.f, -15.f, 0.f);
	camera.line->drawLine(camera, src, dest, color);
}

void Bullet::process() {
	Entity::process();
	life -= 1.f;
	if (life <= 0.f) {
		dead = true;
	}
}

void Bullet::draw(Camera& camera) {
	Rect<int> dest;
	dest.x = pos.x - 2 + game->boardW / 2.f;
	dest.y = pos.y - 2 + game->boardH / 2.f;
	dest.w = 4;
	dest.h = 4;

	Image* image = mainEngine->getImageResource().dataForString("images/system/white.png");
	const WideVector color = team == Team::TEAM_ALLY ? WideVector(0.f, 1.f, 0.f, 1.f) : WideVector(1.f, 0.f, 0.f, 1.f);
	image->drawColor(nullptr, dest, color);
}

void Explosion::process() {
	Entity::process();

	if (ticks < 25.f) {
		radius = ticks;
	} else {
		radius = 25.f - (ticks - 25.f);
	}
	life += 1.f;
	if (life >= 50.f) {
		dead = true;
	}
}

void Explosion::draw(Camera& camera) {
	float angle = game->rand.getFloat() * PI * 2.f;
	static const int steps = 15;
	static const float finc = (PI * 2.f) / (float)steps;
	for (int c = 0; c < steps; ++c) {
		Vector src = pos;
		Vector dest = pos;

		src.x += cosf(angle) * radius;
		src.y += sinf(angle) * radius;
		dest.x += cosf(angle + finc) * radius;
		dest.y += sinf(angle + finc) * radius;

		float r = 0.5f + game->rand.getFloat() / 2.f;
		float g = 0.5f + game->rand.getFloat() / 2.f;
		camera.line->drawLine(camera, src, dest, WideVector(r, g, 0.f, 1.f));
		angle += finc;
	}
}