// Game.hpp

#pragma once

#include "Main.hpp"
#include "Vector.hpp"
#include "LinkedList.hpp"
#include "Camera.hpp"
#include "Random.hpp"

class Game;

class Entity {
public:
	Entity(Game* _game) : game(_game) {}
	virtual ~Entity() {}

	// type
	enum class Type {
		TYPE_PLAYER,
		TYPE_ASTEROID,
		TYPE_ALIEN,
		TYPE_BULLET,
		TYPE_EXPLOSION
	};

	// team
	enum class Team {
		TEAM_NONE,
		TEAM_ALLY,
		TEAM_ENEMY,
		TEAM_MAX
	};

	// get entity type
	virtual const Type getType() const = 0;

	// point value
	virtual const Uint32 getPoints() const { return 0; }

	// update the entity
	virtual void process();

	// draw the entity
	// @param camera The camera to draw with
	virtual void draw(Camera& camera) = 0;

	// called when this object collides with another object
	virtual bool onHit(const Entity* other);

	// shoot a bullet
	void shootBullet(float speed, float range);

	Vector pos;
	Vector vel;
	float ang = 0.f;
	float radius = 0.f;
	float life = 0.f;
	Uint32 ticks = 0;
	Team team = Team::TEAM_NONE;
	bool dead = false;
	const Entity* lastEntityHit = nullptr;
	Game* game = nullptr;
};

class Player : public Entity {
public:
	Player(Game* _game) : Entity(_game) {}
	virtual ~Player() {}

	// get entity type
	virtual const Entity::Type getType() const { return Entity::Type::TYPE_PLAYER; }

	// update the entity
	virtual void process() override;

	// draw the entity
	// @param camera The camera to draw with
	virtual void draw(Camera& camera) override;

	virtual bool onHit(const Entity* other) override;

	Uint32 shootTime = 0;
};

class Asteroid : public Entity {
public:
	Asteroid(Game* _game) : Entity(_game) {}
	virtual ~Asteroid() {}

	// get entity type
	virtual const Entity::Type getType() const { return Entity::Type::TYPE_ASTEROID; }

	// update the entity
	virtual void process() override;

	// draw the entity
	// @param camera The camera to draw with
	virtual void draw(Camera& camera) override;

	virtual bool onHit(const Entity* other) override;

	// point value
	virtual const Uint32 getPoints() const { return 10; }
};

class Alien : public Entity {
public:
	Alien(Game* _game) : Entity(_game) {}
	virtual ~Alien();

	// get entity type
	virtual const Entity::Type getType() const { return Entity::Type::TYPE_ALIEN; }

	// update the entity
	virtual void process() override;

	// draw the entity
	// @param camera The camera to draw with
	virtual void draw(Camera& camera) override;

	virtual bool onHit(const Entity* other) override;

	enum class State {
		GO_DIAGONAL,
		GO_STRAIGHT
	};

	State state = State::GO_STRAIGHT;
	int channel = -1;

	// point value
	virtual const Uint32 getPoints() const { return 50; }
};

class Bullet : public Entity {
public:
	Bullet(Game* _game) : Entity(_game) {}
	virtual ~Bullet() {}

	// get entity type
	virtual const Entity::Type getType() const { return Entity::Type::TYPE_BULLET; }

	// update the entity
	virtual void process() override;

	// draw the entity
	// @param camera The camera to draw with
	virtual void draw(Camera& camera) override;
};

class Explosion : public Entity {
public:
	Explosion(Game* _game) : Entity(_game) {}
	virtual ~Explosion() {}

	// get entity type
	virtual const Entity::Type getType() const { return Entity::Type::TYPE_EXPLOSION; }

	// update the entity
	virtual void process() override;

	// draw the entity
	// @param camera The camera to draw with
	virtual void draw(Camera& camera) override;
};

class Game {
public:
	Game();
	~Game();

	// init
	void init(float _boardW, float _boardH);

	// count asteroids
	int countAsteroids();

	// spawn asteroids
	void spawnAsteroids();

	// spawn player
	void spawnPlayer();

	// term
	void term();

	// lets you control the game with a keyboard
	void doKeyboardInput();

	// process a frame
	void process();

	// draw a frame
	void draw(Camera& camera);

	// add entity to gamestate
	void addEntity(Entity* entity);

	LinkedList<Entity*> entities;
	Player* player = nullptr;

	// inputs
	enum Input {
		IN_THRUST,
		IN_RIGHT,
		IN_LEFT,
		IN_SHOOT,
		IN_MAX
	};
	bool inputs[IN_MAX];

	int wins = 0;
	int losses = 0;
	float boardW = 0.f;
	float boardH = 0.f;
	float wonTimer = 0.f;
	float lossTimer = -1.f;
	Uint32 beat = 60;
	bool previousBeat = false;
	Uint32 score = 0;
	Uint32 lives = 3;
	Random rand;

	Uint32 ticks = 0;
};