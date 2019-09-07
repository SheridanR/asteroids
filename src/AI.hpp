// AI.hpp

#pragma once

#include "Main.hpp"
#include "ArrayList.hpp"
#include "String.hpp"
#include "Map.hpp"
#include "Random.hpp"
#include "File.hpp"
#include "Pair.hpp"

#include <memory>
#include <atomic>
#include <future>
#include <vector>

class Game;
class Neuron;
class Network;
class Gene;
class Genome;
class Species;
class Pool;
class AI;

class Pool {
public:
	Pool();

	void init();

	int newInnovation();

	void rankGlobally();

	int64_t totalAverageFitness();

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

	int generation = 0;
	int innovation;
	std::atomic<int64_t> maxFitness { 0 };
	ArrayList<Species> species;
	int inputSize = 0;
	Random rand;

	AI* ai = nullptr;
};

class AI {
public:
	AI();
	~AI();

	// getters
	int getGeneration() const { return pool ? pool->generation : 0; }
	int64_t getMaxFitness() const { return pool ? pool->maxFitness.load() : 0; }
	int getMeasured() const;

	// setup
	void init();

	// step the AI one frame
	// @return true if a genome's fitness was still being measured, otherwise false
	bool process();

	// save AI
	void save();

	// load AI
	void load();

	// play the best attempt
	void playTop();

	// advance generation
	void nextGeneration();

	static const int Outputs;

	static const int Population;
	static const float DeltaDisjoint;
	static const float DeltaWeights;
	static const float DeltaThreshold;
	static const float TicksPerSecond;

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

	static const int MaxNodes;

	std::shared_ptr<Game> focus { nullptr };

private:
	Pool* pool = nullptr;
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
	Gene(const Gene& src) :
		into(src.into),
		out(src.out),
		weight(src.weight),
		enabled(src.enabled),
		innovation(src.innovation)
	{}

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
	Genome(const Genome& src);

	Genome& operator=(const Genome& src);

	void mutate();

	int randomNeuron(bool nonInput);

	bool containsLink(const Gene& link);

	void pointMutate();

	void linkMutate(bool forceBias);

	void nodeMutate();

	void generateNetwork();

	void enableDisableMutate(bool enable);

	ArrayList<float> evaluateNetwork(ArrayList<float>& inputs);

	void initializeRun();

	void clearJoypad();

	void evaluateCurrent();

	ArrayList<float> getInputs();

	// save/load this object to a file
	// @param file interface to serialize with
	void serialize(FileInterface * file);

	ArrayList<Gene> genes;
	int64_t fitness = 0;
	Network network;
	int maxNeuron = 0;
	int globalRank = 0;
	Map<String, float> mutationRates;

	Pool* pool = nullptr;

	int framesSurvived = 0;
	Uint32 currentFrame = 0;
	std::shared_ptr<Game> game { nullptr };
	bool finished = false;
	float totalDanger = 0.f;

	// controller outputs
	enum Output {
		OUT_THRUST,
		OUT_RIGHT,
		OUT_LEFT,
		//OUT_SHOOT,
		OUT_MAX
	};
	float outputs[Output::OUT_MAX];

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

private:
	static float sigmoid(float x);
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

	int64_t topFitness = 0;
	int staleness = 0;
	int64_t averageFitness = 0;
	ArrayList<Genome> genomes;

	Pool* pool = nullptr;
};