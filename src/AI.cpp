// AI.cpp

#include "Main.hpp"
#include "AI.hpp"
#include "Engine.hpp"
#include "Game.hpp"

const int AI::BoxRadius = 100;
const int AI::Outputs = OUT_MAX;

const int AI::Population = 300;
const float AI::DeltaDisjoint = 2.f;
const float AI::DeltaWeights = 0.4f;
const float AI::DeltaThreshold = 1.f;

const int AI::StaleSpecies = 15;

const float AI::MutateConnectionsChance = 0.25f;
const float AI::PerturbChance = 0.90f;
const float AI::CrossoverChance = 0.75f;
const float AI::LinkMutationChance = 2.0f;
const float AI::NodeMutationChance = 0.50f;
const float AI::BiasMutationChance = 0.40f;
const float AI::StepSize = 0.1f;
const float AI::DisableMutationChance = 0.4f;
const float AI::EnableMutationChance = 0.2f;

const int AI::TimeoutConstant = 20;

const int AI::MaxNodes = 1000000;

Gene Gene::copy() {
	Gene gene;
	gene.into = into;
	gene.out = out;
	gene.weight = weight;
	gene.enabled = enabled;
	gene.innovation = innovation;
	return gene;
}

void Gene::serialize(FileInterface* file) {
	int version = 0;
	file->property("version", version);
	file->property("into", into);
	file->property("out", out);
	file->property("weight", weight);
	file->property("innovation", innovation);
	file->property("enabled", enabled);
}

Genome::Genome() {
	mutationRates.insert("connections", AI::MutateConnectionsChance);
	mutationRates.insert("link", AI::LinkMutationChance);
	mutationRates.insert("bias", AI::BiasMutationChance);
	mutationRates.insert("node", AI::NodeMutationChance);
	mutationRates.insert("enable", AI::EnableMutationChance);
	mutationRates.insert("disable", AI::DisableMutationChance);
	mutationRates.insert("step", AI::StepSize);
}

Genome Genome::copy() {
	Genome genome;
	genome.pool = pool;
	for (int c = 0; c < genes.getSize(); ++c) {
		genome.genes.copy(genes);
	}
	genome.maxNeuron = maxNeuron;
	*genome.mutationRates["connections"] = *genome.mutationRates["connections"];
	*genome.mutationRates["link"] = *genome.mutationRates["link"];
	*genome.mutationRates["bias"] = *genome.mutationRates["bias"];
	*genome.mutationRates["node"] = *genome.mutationRates["node"];
	*genome.mutationRates["enable"] = *genome.mutationRates["enable"];
	*genome.mutationRates["disable"] = *genome.mutationRates["disable"];
	return genome;
}

void Genome::generateNetwork() {
	for (int c = 0; c < pool->inputSize; ++c) {
		network.neurons.insert(c, Neuron());
	}

	for (int c = 0; c < AI::Outputs; ++c) {
		network.neurons.insert(AI::MaxNodes + c, Neuron());
	}

	// -.- sort function required
	genes.sort(Gene::AscSort());
	for (int i = 1; i < genes.getSize(); ++i) {
		auto& gene = genes[i];
		if (gene.enabled) {
			if (network.neurons[gene.out] == nullptr) {
				network.neurons.insert(gene.out, Neuron());
			}
			Neuron& neuron = *network.neurons[gene.out];
			neuron.incoming.push(gene);
			if (network.neurons[gene.into] == nullptr) {
				network.neurons.insert(gene.into, Neuron());
			}
		}
	}
}

ArrayList<bool> Genome::evaluateNetwork(ArrayList<int>& inputs) {
	if (inputs.getSize() != pool->inputSize) {
		mainEngine->fmsg(Engine::MSG_WARN, "incorrect number of neural network inputs");
		return ArrayList<bool>();
	}

	for (int i = 0; i < pool->inputSize; ++i) {
		Neuron* neuron = network.neurons[i];
		if (neuron) {
			neuron->value = inputs[i];
		}
	}

	for (auto& pair : network.neurons) {
		auto& neuron = pair.b;
		int sum = 0;
		for (int j = 0; j < neuron.incoming.getSize(); ++j) {
			auto& incoming = neuron.incoming[j];
			auto other = network.neurons[incoming.into];
			if (other) {
				sum += incoming.weight * other->value;
			}
		}

		if (neuron.incoming.getSize()) {
			neuron.value = AI::sigmoid(sum);
		}
	}

	ArrayList<bool> outputs;
	outputs.resize(AI::Outputs);
	for (int o = 0; o < AI::Outputs; ++o) {
		Neuron* neuron = network.neurons[AI::MaxNodes + o];
		if (neuron->value > 0) {
			outputs[o] = true;
		} else {
			outputs[o] = false;
		}
	}

	return outputs;
}

int Genome::randomNeuron(bool nonInput) {
	Map<int, bool> neurons;

	if (!nonInput) {
		for (int i = 0; i < pool->inputSize; ++i) {
			neurons.insert(i, true);
		}
	}

	for (int o = 0; o < AI::Outputs; ++o) {
		neurons.insert(AI::MaxNodes + o, true);
	}

	for (int i = 0; i < genes.getSize(); ++i) {
		if (!nonInput || genes[i].into > pool->inputSize) {
			bool* neuron = neurons[genes[i].into];
			if (!neuron) {
				neurons.insert(genes[i].into, true);
			} else {
				*neuron = true;
			}
		}
		if (!nonInput || genes[i].out > pool->inputSize) {
			bool* neuron = neurons[genes[i].out];
			if (!neuron) {
				neurons.insert(genes[i].out, true);
			} else {
				*neuron = true;
			}
		}
	}

	if (!neurons.getSize()) {
		return 0;
	}

	int n = pool->rand.getUint32() % neurons.getSize();
	for (auto& pair : neurons) {
		if (n == 0) {
			return pair.a;
		}
		--n;
	}

	return 0;
}

bool Genome::containsLink(const Gene& link) {
	for (int i = 0; i < genes.getSize(); ++i) {
		auto& gene = genes[i];
		if (gene.into == link.into && gene.out == link.out) {
			return true;
		}
	}
	return false;
}

void Genome::pointMutate() {
	auto step = *mutationRates["step"];

	for (int i = 0; i < genes.getSize(); ++i) {
		auto& gene = genes[i];
		if (pool->rand.getFloat() < AI::PerturbChance) {
			gene.weight = gene.weight + pool->rand.getFloat() * step * 2.f - step;
		} else {
			gene.weight = pool->rand.getFloat() * 4.f - 2.f;
		}
	}
}

void Genome::linkMutate(bool forceBias) {
	auto neuron1 = randomNeuron(false);
	auto neuron2 = randomNeuron(true);

	Gene newLink;
	if (neuron1 <= pool->inputSize && neuron2 <= pool->inputSize) {
		// both input nodes
		return;
	}

	if (neuron2 <= pool->inputSize) {
		// swap input and output
		auto temp = neuron1;
		neuron1 = neuron2;
		neuron2 = temp;
	}

	newLink.into = neuron1;
	newLink.out = neuron2;
	if (forceBias) {
		newLink.into = pool->inputSize;
	}

	if (containsLink(newLink)) {
		return;
	}
	assert(pool);
	newLink.innovation = pool->newInnovation();
	newLink.weight = pool->rand.getFloat() * 4.f - 2.f;
	genes.push(newLink);
}

void Genome::nodeMutate() {
	if (genes.getSize() == 0) {
		return;
	}

	++maxNeuron;

	auto& gene = genes[pool->rand.getUint32() % genes.getSize()];
	if (!gene.enabled) {
		return;
	}
	gene.enabled = false;

	auto gene1 = gene.copy();
	gene1.out = maxNeuron;
	gene1.weight = 1.f;
	gene1.innovation = pool->newInnovation();
	gene1.enabled = true;
	genes.push(gene1);

	auto gene2 = gene.copy();
	gene2.into = maxNeuron;
	gene2.innovation = pool->newInnovation();
	gene2.enabled = true;
	genes.push(gene2);
}

void Genome::enableDisableMutate(bool enable) {
	ArrayList<Gene*> candidates;
	for (auto& gene : genes) {
		if (gene.enabled != enable) {
			candidates.push(&gene);
		}
	}

	if (candidates.getSize() == 0) {
		return;
	}

	auto gene = candidates[pool->rand.getUint32() % candidates.getSize()];
	gene->enabled = !gene->enabled;
}

void Genome::mutate() {
	for (auto& pair : mutationRates) {
		if (pool->rand.getUint32() % 2 == 0) {
			pair.b *= 0.95f;
		} else {
			pair.b *= 1.05263f;
		}
	}

	if (pool->rand.getFloat() < *mutationRates["connections"]) {
		pointMutate();
	}

	{
		float p = *mutationRates["link"];
		while (p > 0.f) {
			if (pool->rand.getFloat() < p) {
				linkMutate(false);
			}
			p -= 1.f;
		}
	}

	{
		float p = *mutationRates["bias"];
		while (p > 0.f) {
			if (pool->rand.getFloat() < p) {
				linkMutate(true);
			}
			p -= 1.f;
		}
	}

	{
		float p = *mutationRates["node"];
		while (p > 0.f) {
			if (pool->rand.getFloat() < p) {
				nodeMutate();
			}
			p -= 1.f;
		}
	}

	{
		float p = *mutationRates["enable"];
		while (p > 0.f) {
			if (pool->rand.getFloat() < p) {
				enableDisableMutate(true);
			}
			p -= 1.f;
		}
	}

	{
		float p = *mutationRates["disable"];
		while (p > 0.f) {
			if (pool->rand.getFloat() < p) {
				enableDisableMutate(false);
			}
			p -= 1.f;
		}
	}
}

void Genome::serialize(FileInterface* file) {
	int version = 0;
	file->property("version", version);
	file->property("fitness", fitness);
	file->property("maxNeuron", maxNeuron);
	file->property("mutationRates", mutationRates);
	file->property("genes", genes);
}

Genome Species::crossover(Genome* g1, Genome* g2) {
	assert(g1);
	assert(g2);

	// make sure g1 is the higher fitness genome
	if (g2->fitness > g1->fitness) {
		auto temp = g1;
		g1 = g2;
		g2 = temp;
	}

	Genome child;
	child.pool = pool;

	Map<int, Gene> innovations2;
	for (int i = 0; i < g2->genes.getSize(); ++i) {
		auto& gene = g2->genes[i];
		innovations2.insert(gene.innovation, gene);
	}

	for (int i = 0; i < g1->genes.getSize(); ++i) {
		auto& gene1 = g1->genes[i];
		auto gene2 = innovations2[gene1.innovation];
		if (gene2 != nullptr && pool->rand.getUint8()%2 == 0 && gene2->enabled) {
			child.genes.push(gene2->copy());
		} else {
			child.genes.push(gene1.copy());
		}
	}

	child.maxNeuron = std::max(g1->maxNeuron, g2->maxNeuron);

	for (auto& pair : g1->mutationRates) {
		auto mutation = child.mutationRates[pair.a];
		if (mutation) {
			*mutation = pair.b;
		}
	}

	return child;
}

int Species::disjoint(Genome* g1, Genome* g2) {
	assert(g1);
	assert(g2);

	Map<int, bool> i1;
	for (int i = 0; i < g1->genes.getSize(); ++i) {
		auto& gene = g1->genes[i];
		i1.insert(gene.innovation, true);
	}

	Map<int, bool> i2;
	for (int i = 0; i < g2->genes.getSize(); ++i) {
		auto& gene = g2->genes[i];
		i2.insert(gene.innovation, true);
	}

	int result = 0;

	for (int i = 0; i < g1->genes.getSize(); ++i) {
		auto& gene = g1->genes[i];
		if (!i2[gene.innovation]) {
			++result;
		}
	}

	for (int i = 0; i < g2->genes.getSize(); ++i) {
		auto& gene = g2->genes[i];
		if (!i1[gene.innovation]) {
			++result;
		}
	}

	int n = (int)std::max(g1->genes.getSize(), g2->genes.getSize());
	if (n) {
		return result / n;
	} else {
		return 0;
	}
}

int Species::weights(Genome* g1, Genome* g2) {
	assert(g1);
	assert(g2);

	Map<int, Gene*> i2;
	for (int i = 0; i < g2->genes.getSize(); ++i) {
		auto& gene = g2->genes[i];
		i2.insert(gene.innovation, &gene);
	}

	int sum = 0;
	int coincident = 0;
	for (int i = 0; i < g1->genes.getSize(); ++i) {
		auto& gene = g1->genes[i];
		if (i2[gene.innovation] != nullptr) {
			auto& gene2 = **i2[gene.innovation];
			sum = sum + fabs(gene.weight - gene2.weight);
			++coincident;
		}
	}

	if (coincident) {
		return sum / coincident;
	} else {
		return 0;
	}
}

bool Species::sameSpecies(Genome* g1, Genome* g2) {
	assert(g1);
	assert(g2);

	float dd = AI::DeltaDisjoint * disjoint(g1, g2);
	float dw = AI::DeltaWeights * weights(g1, g2);
	return dd + dw < AI::DeltaThreshold;
}

void Species::calculateAverageFitness() {
	int total = 0;

	for (int g = 0; g < genomes.getSize(); ++g) {
		auto& genome = genomes[g];
		total += genome.globalRank;
	}
	if (genomes.getSize()) {
		averageFitness = total / (int)genomes.getSize();
	} else {
		averageFitness = 0;
	}
}

Genome Species::breedChild() {
	Genome child;
	child.pool = pool;
	if (genomes.getSize()) {
		if (pool->rand.getFloat() < AI::CrossoverChance) {
			auto& g1 = genomes[pool->rand.getUint32() % genomes.getSize()];
			auto& g2 = genomes[pool->rand.getUint32() % genomes.getSize()];
			child = crossover(&g1, &g2);
		} else {
			auto& g = genomes[pool->rand.getUint32() % genomes.getSize()];
			child = g.copy();
		}
	} else {
		assert(0); // what the heck!
	}

	child.mutate();

	return child;
}

void Species::serialize(FileInterface* file) {
	int version = 0;
	file->property("version", version);
	file->property("topFitness", topFitness);
	file->property("staleness", staleness);
	file->property("genomes", genomes);
}

Pool::Pool() {
	innovation = AI::Outputs;
}

void Pool::init() {
	for (int c = 0; c < AI::Population; ++c) {
		Genome genome;
		genome.pool = this;
		genome.maxNeuron = inputSize;
		genome.mutate();
		addToSpecies(genome);
	}
}

int Pool::newInnovation() {
	++innovation;
	return innovation;
}

void Pool::rankGlobally() {
	ArrayList<Genome*> global;
	for (int s = 0; s < species.getSize(); ++s) {
		auto& spec = species[s];
		for (int g = 0; g < spec.genomes.getSize(); ++g) {
			global.push(&spec.genomes[g]);
		}
	}
	global.sort(Genome::AscSortPtr());

	for (int g = 0; g < global.getSize(); ++g) {
		global[g]->globalRank = g;
	}
}

int Pool::totalAverageFitness() {
	int total = 0;
	for (int s = 0; s < species.getSize(); ++s) {
		auto& spec = species[s];
		total += spec.averageFitness;
	}
	return total;
}

void Pool::cullSpecies(bool cutToOne) {
	for (int s = 0; s < species.getSize(); ++s) {
		auto& spec = species[s];

		spec.genomes.sort(Genome::DescSort());

		int remaining = (int)ceilf(spec.genomes.getSize() / 2.f);
		if (cutToOne) {
			remaining = 1;
		}
		while (spec.genomes.getSize() > remaining) {
			spec.genomes.pop();
		}
	}
}

void Pool::removeStaleSpecies() {
	ArrayList<Species> survived;
	for (int s = 0; s < species.getSize(); ++s) {
		auto& spec = species[s];
		
		if (spec.genomes.getSize() == 0) {
			++spec.staleness;
		} else {
			spec.genomes.sort(Genome::DescSort());

			if (spec.genomes[0].fitness > spec.topFitness) {
				spec.topFitness = spec.genomes[0].fitness;
				spec.staleness = 0;
			} else {
				++spec.staleness;
			}
		}
		if (spec.staleness < AI::StaleSpecies || spec.topFitness >= maxFitness) {
			survived.push(spec);
		}
	}
	species.swap(survived);
}

void Pool::removeWeakSpecies() {
	ArrayList<Species> survived;
	int sum = totalAverageFitness();
	for (int s = 0; s < species.getSize(); ++s) {
		auto& spec = species[s];
		int breed = sum ? (int)floorf(((float)spec.averageFitness / sum) * AI::Population) : 0;
		if (breed >= 1) {
			survived.push(spec);
		}
	}
	species.swap(survived);
}

void Pool::addToSpecies(Genome& child) {
	bool foundSpecies = false;
	for (int s = 0; s < species.getSize(); ++s) {
		auto& spec = species[s];
		if (spec.sameSpecies(&child, &spec.genomes[0])) {
			spec.genomes.push(child);
			foundSpecies = true;
			break;
		}
	}
	if (!foundSpecies) {
		Species childSpecies;
		childSpecies.pool = this;
		childSpecies.genomes.push(child);
		species.push(childSpecies);
	}
}

void Pool::newGeneration() {
	cullSpecies(false); // cull the bottom half of each species
	rankGlobally();
	removeStaleSpecies();
	rankGlobally();
	for (int s = 0; s < species.getSize(); ++s) {
		auto& spec = species[s];
		spec.calculateAverageFitness();
	}
	removeWeakSpecies();
	int sum = totalAverageFitness();
	ArrayList<Genome> children;
	for (int s = 0; s < species.getSize(); ++s) {
		auto& spec = species[s];
		int breed = (int)floorf(((float)spec.averageFitness / sum) * AI::Population) - 1;
		for (int i = 1; i <= breed; ++i) {
			children.push(spec.breedChild());
		}
	}
	cullSpecies(true); // cull all but the top member of each species
	while (children.getSize() + species.getSize() < AI::Population) {
		auto& spec = species[rand.getUint32() % species.getSize()];
		children.push(spec.breedChild());
	}
	for (int c = 0; c < children.getSize(); ++c) {
		auto& child = children[c];
		addToSpecies(child);
	}

	++generation;

	StringBuf<32> buf("backup%d.json",generation);
	writeFile(buf.get());
}

void Pool::writeFile(const char* filename) {
	FileHelper::writeObject(filename, EFileFormat::Json, *this);
}

void Pool::savePool() {
	const char* filename = "pool.json";
	writeFile(filename);
}

void Pool::loadFile(const char* filename) {
	currentFrame = 0;
	currentSpecies = 0;
	currentGenome = 0;
	generation = 0;
	innovation = AI::Outputs;
	maxFitness = 0;
	species.clear();
	FileHelper::readObject(filename, *this);
}

void Pool::loadPool() {
	const char* filename = "pool.json";
	loadFile(filename);
}

void Pool::serialize(FileInterface* file) {
	int version = 0;
	file->property("version", version);
	file->property("generation", generation);
	file->property("maxFitness", maxFitness);
	file->property("species", species);
	if (file->isReading()) {
		for (auto& spec : species) {
			spec.pool = this;
			for (auto& genome : spec.genomes) {
				genome.pool = this;
			}
		}
	}
}

AI::AI(Game* _game) {
	game = _game;
}

AI::~AI() {
	if (pool) {
		delete pool;
		pool = nullptr;
	}
}

void AI::init(int boardW, int boardH) {
	if (pool) {
		delete pool;
		pool = nullptr;
	}
	pool = new Pool();
	pool->rand.seedTime();
	pool->boardW = boardW / BoxRadius;
	pool->boardH = boardH / BoxRadius;
	pool->inputSize = pool->boardW * pool->boardH;
	pool->init();
	pool->writeFile("temp.json");
	initializeRun();
}

float AI::sigmoid(float x) {
	return 2.f / (1.f + expf(-4.9f * x)) - 1.f;
}

ArrayList<int> AI::getInputs() {
	assert(game);

	ArrayList<int> inputs;
	inputs.resize(pool->inputSize);

	int startY = (-pool->boardH / 2) * BoxRadius;
	int startX = (-pool->boardW / 2) * BoxRadius;
	int endY = (pool->boardH / 2) * BoxRadius;
	int endX = (pool->boardW / 2) * BoxRadius;

	int index = 0;
	for (int dy = startY; dy < endY; dy += BoxRadius) {
		for (int dx = startX; dx < endX; dx += BoxRadius) {
			inputs[index] = 0;
			for (auto entity : game->entities) {
				float distx = (fabs(entity->pos.x - dx)) - entity->radius;
				float disty = (fabs(entity->pos.y - dy)) - entity->radius;
				if (distx <= 8 && disty <= 8) {
					inputs[index] = entity->team == Entity::Team::TEAM_ALLY ? 1 : -1;
				}
			}
			++index;
		}
	}

	return inputs;
}

void AI::clearJoypad() {
	for (int c = 0; c < (int)Output::OUT_MAX; ++c) {
		outputs[c] = false;
	}
}

void AI::initializeRun() {
	game->term();
	game->init();
	framesSurvived = 0;
	timeout = TimeoutConstant;
	pool->currentFrame = 0;
	clearJoypad();
	auto& spec = pool->species[pool->currentSpecies];
	auto& genome = spec.genomes[pool->currentGenome];
	genome.generateNetwork();
	evaluateCurrent();
}

void AI::evaluateCurrent() {
	auto& spec = pool->species[pool->currentSpecies];
	auto& genome = spec.genomes[pool->currentGenome];

	auto inputs = getInputs();
	auto controller = genome.evaluateNetwork(inputs);

	if (controller.getSize()) {
		if (controller[Output::OUT_LEFT] && controller[Output::OUT_RIGHT]) {
			controller[Output::OUT_LEFT] = false;
			controller[Output::OUT_RIGHT] = false;
		}
		for (int c = 0; c < (int)Output::OUT_MAX; ++c) {
			outputs[c] = controller[c];
		}
	} else {
		for (int c = 0; c < (int)Output::OUT_MAX; ++c) {
			outputs[c] = false;
		}
	}
}

void AI::nextGenome() {
	++pool->currentGenome;
	if (pool->currentGenome >= pool->species[pool->currentSpecies].genomes.getSize()) {
		pool->currentGenome = 0;
		++pool->currentSpecies;
		if (pool->currentSpecies >= pool->species.getSize()) {
			pool->newGeneration();
			pool->currentSpecies = 0;
		}
	}
}

bool AI::fitnessAlreadyMeasured() {
	auto& spec = pool->species[pool->currentSpecies];
	auto& genome = spec.genomes[pool->currentGenome];
	return genome.fitness != 0;
}

void AI::playTop() {
	int maxFitness = 0;
	int maxs = 0, maxg = 0;
	for (int s = 0; s < pool->species.getSize(); ++s) {
		auto& spec = pool->species[s];
		for (int g = 0; g < spec.genomes.getSize(); ++g) {
			auto& genome = spec.genomes[g];
			if (genome.fitness > maxFitness) {
				maxFitness = genome.fitness;
				maxs = s;
				maxg = g;
			}
		}
	}

	pool->currentSpecies = maxs;
	pool->currentGenome = maxg;
	pool->maxFitness = maxFitness;
	initializeRun();
	++pool->currentFrame;
}

void AI::process() {
	Species& species = pool->species[pool->currentSpecies];
	Genome& genome = species.genomes[pool->currentGenome];

	if (pool->currentFrame % 5 == 0) {
		evaluateCurrent();
	}

	if (game->player) {
		if (game->player->moved && (int)game->player->ticks > framesSurvived) {
			framesSurvived = game->player->ticks;
			timeout = TimeoutConstant;
		}
	}

	--timeout;

	int timeoutBonus = pool->currentFrame / 4;
	if (timeout + timeoutBonus <= 0) {
		int fitness = framesSurvived - pool->currentFrame / 2;
		fitness += game->score + game->wins * 1000;
		fitness -= game->losses * 100;
		// can do additional fitness bonuses here
		if (fitness == 0) {
			fitness = -1;
		}
		genome.fitness = fitness;

		if (fitness > pool->maxFitness) {
			pool->maxFitness = fitness;
		}

		pool->currentSpecies = 0;
		pool->currentGenome = 0;
		while (fitnessAlreadyMeasured()) {
			nextGenome();
		}
		initializeRun();
	}

	int measured = 0;
	int total = 0;
	for (auto& spec : pool->species) {
		for (auto& genome : spec.genomes) {
			++total;
			if (genome.fitness != 0) {
				++measured;
			}
		}
	}
	++pool->currentFrame;
}

void AI::save() {
	pool->savePool();
}

void AI::load() {
	pool->loadPool();
	while (fitnessAlreadyMeasured()) {
		nextGenome();
	}
	initializeRun();
	++pool->currentFrame;
}