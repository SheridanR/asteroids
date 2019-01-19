// AI.hpp

#pragma once

#include "Main.hpp"
#include "ArrayList.hpp"
#include "String.hpp"
#include "Map.hpp"
#include "Random.hpp"
#include "File.hpp"

class Game;
class Neuron;
class Network;
class Gene;
class Genome;
class Species;
class Pool;

class Pool {
public:
	Pool();

	void init();

	int newInnovation();

	void rankGlobally();

	int totalAverageFitness();

	void cullSpecies(bool cutToOne);

	void removeStaleSpecies();

	void removeWeakSpecies();

	void addToSpecies(Genome& child);

	void newGeneration();

	void loadPool();

	void loadFile(const char* filename);

	void savePool();

	void writeFile(const char* filename);

	// save/load this object to a file
	// @param file interface to serialize with
	void serialize(FileInterface * file);

	Random rand;
	Uint32 currentFrame = 0;
	int currentSpecies = 0; // index from 0
	int currentGenome = 0; // index from 0
	int generation = 0;
	int innovation;
	int maxFitness = 0;
	ArrayList<Species> species;

	int inputSize = 0;
	int boardW = 0, boardH = 0;
};

class AI {
public:
	AI(Game* _game);
	~AI();

	// getters
	int getGeneration() const { return pool ? pool->generation : 0; }
	int getSpecies() const { return pool ? pool->currentSpecies : 0; }
	int getGenome() const { return pool ? pool->currentGenome : 0; }
	int getMaxFitness() const { return pool ? pool->maxFitness : 0; }

	// setup
	void init(int boardW, int boardH);

	// step the AI one frame
	void process();

	// save AI
	void save();

	// load AI
	void load();

	void playTop();

	static float sigmoid(float x);

	ArrayList<float> getInputs();

	void clearJoypad();

	void initializeRun();

	void evaluateCurrent();

	void nextGenome();

	bool fitnessAlreadyMeasured();

	static const int BoxRadius;
	static const int Outputs;

	// controller outputs
	enum Output {
		OUT_THRUST,
		OUT_RIGHT,
		OUT_LEFT,
		OUT_SHOOT,
		OUT_MAX
	};
	bool outputs[Output::OUT_MAX];

	static const int Population;
	static const float DeltaDisjoint;
	static const float DeltaWeights;
	static const float DeltaThreshold;

	static const int StaleSpecies;

	static const float MutateConnectionsChance;
	static const float PerturbChance;
	static const float CrossoverChance;
	static const float LinkMutationChance;
	static const float NodeMutationChance;
	static const float BiasMutationChance;
	static const float StepSize;
	static const float DisableMutationChance;
	static const float EnableMutationChance;

	static const int TimeoutConstant;

	static const int MaxNodes;

private:
	Game* game = nullptr;
	Pool* pool = nullptr;

	int timeout = 0;
	int framesSurvived = 0;
};

class Neuron {
public:
	Neuron() {}

	ArrayList<Gene*> incoming;
	float value = 0.f;
};

class Network {
public:
	Network() {}

	Map<int, Neuron> neurons;
};

class Gene {
public:
	Gene() {}

	Gene copy();

	// save/load this object to a file
	// @param file interface to serialize with
	void serialize(FileInterface * file);

	int into = 0;
	int out = 0;
	float weight = 0.f;
	bool enabled = true;
	int innovation = 0;

	class AscSort : public ArrayList<Gene>::SortFunction {
	public:
		AscSort() {}
		virtual ~AscSort() {}

		virtual const bool operator()(const Gene& a, const Gene& b) const override {
			return a.out < b.out;
		}
	};

	class DescSort : public ArrayList<Gene>::SortFunction {
	public:
		DescSort() {}
		virtual ~DescSort() {}

		virtual const bool operator()(const Gene& a, const Gene& b) const override {
			return a.out > b.out;
		}
	};
};

class Genome {
public:
	Genome();

	Genome copy();

	void mutate();

	int randomNeuron(bool nonInput);

	bool containsLink(const Gene& link);

	void pointMutate();

	void linkMutate(bool forceBias);

	void nodeMutate();

	void generateNetwork();

	void enableDisableMutate(bool enable);

	ArrayList<bool> evaluateNetwork(ArrayList<float>& inputs);

	// save/load this object to a file
	// @param file interface to serialize with
	void serialize(FileInterface * file);

	ArrayList<Gene> genes;
	int fitness = 0;
	int adjustedFitness = 0;
	Network network;
	int maxNeuron = 0;
	int globalRank = 0;
	Map<String, float> mutationRates;

	Pool* pool = nullptr;

	class AscSortPtr : public ArrayList<Genome*>::SortFunction {
	public:
		AscSortPtr() {}
		virtual ~AscSortPtr() {}

		virtual const bool operator()(Genome *const & a, Genome *const & b) const override {
			return a->fitness < b->fitness;
		}
	};

	class DescSort : public ArrayList<Genome>::SortFunction {
	public:
		DescSort() {}
		virtual ~DescSort() {}

		virtual const bool operator()(const Genome& a, const Genome& b) const override {
			return a.fitness > b.fitness;
		}
	};
};

class Species {
public:
	Species() {}

	Genome crossover(Genome* g1, Genome* g2);

	float disjoint(Genome* g1, Genome* g2);

	float weights(Genome* g1, Genome* g2);

	bool sameSpecies(Genome* g1, Genome* g2);

	void calculateAverageFitness();

	Genome breedChild();

	// save/load this object to a file
	// @param file interface to serialize with
	void serialize(FileInterface * file);

	int topFitness = 0;
	int staleness = 0;
	int averageFitness = 0;
	ArrayList<Genome> genomes;

	Pool* pool = nullptr;
};