// AI.cpp

#include "Main.hpp"
#include "AI.hpp"
#include "Engine.hpp"
#include "Game.hpp"

const int AI::Outputs = Genome::Output::OUT_MAX;

const int AI::Population = 300;
const float AI::DeltaDisjoint = 2.f;
const float AI::DeltaWeights = 0.4f;
const float AI::DeltaThreshold = 1.f;
const float AI::TicksPerSecond = 60.f;

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

const int AI::MaxNodes = 1000000;

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

Genome::Genome(const Genome& src) {
	genes.copy(src.genes);
	fitness = src.fitness;
	network = src.network;
	maxNeuron = src.maxNeuron;
	globalRank = src.globalRank;
	mutationRates.copy(src.mutationRates);
	pool = src.pool;
	framesSurvived = src.framesSurvived;
	currentFrame = src.currentFrame;
	game = src.game;
	if (game) {
		game->genome = this;
	}
	finished = src.finished;
	totalDanger = src.totalDanger;
}

Genome& Genome::operator=(const Genome& src) {
	genes.copy(src.genes);
	fitness = src.fitness;
	network = src.network;
	maxNeuron = src.maxNeuron;
	globalRank = src.globalRank;
	mutationRates.copy(src.mutationRates);
	pool = src.pool;
	framesSurvived = src.framesSurvived;
	currentFrame = src.currentFrame;
	game = src.game;
	if (game) {
		game->genome = this;
	}
	finished = src.finished;
	totalDanger = src.totalDanger;
	return *this;
}

void Genome::generateNetwork() {
	network.neurons.clear();

	for (int c = 0; c < pool->inputSize; ++c) {
		network.neurons.insert(c, Neuron());
	}

	for (int c = 0; c < AI::Outputs; ++c) {
		network.neurons.insert(AI::MaxNodes + c, Neuron());
	}

	genes.sort(Gene::AscSort());
	for (int i = 0; i < genes.getSize(); ++i) {
		auto& gene = genes[i];
		if (gene.enabled) {
			if (network.neurons[gene.out] == nullptr) {
				network.neurons.insert(gene.out, Neuron());
			}
			Neuron& neuron = *network.neurons[gene.out];
			neuron.incoming.push(&gene);
			if (network.neurons[gene.into] == nullptr) {
				network.neurons.insert(gene.into, Neuron());
			}
		}
	}
}

float Genome::sigmoid(float x) {
	return 2.f / (1.f + expf(-4.9f * x)) - 1.f;
}

ArrayList<float> Genome::evaluateNetwork(ArrayList<float>& inputs) {
	if (inputs.getSize() != pool->inputSize) {
		mainEngine->fmsg(Engine::MSG_WARN, "incorrect number of neural network inputs");
		return ArrayList<float>();
	}

	for (int i = 0; i < pool->inputSize; ++i) {
		Neuron* neuron = network.neurons[i];
		assert(neuron);
		neuron->value = inputs[i];
	}

	for (auto& pair : network.neurons) {
		auto& neuron = pair.b;
		float sum = 0;
		for (int j = 0; j < neuron.incoming.getSize(); ++j) {
			auto& incoming = neuron.incoming[j];
			auto other = network.neurons[incoming->into];
			assert(other);
			sum += incoming->weight * other->value;
		}

		if (neuron.incoming.getSize()) {
			neuron.value = sigmoid(sum);
		} else {
			neuron.value = 0.f;
		}
	}

	ArrayList<float> outputs;
	outputs.resize(AI::Outputs);
	for (int o = 0; o < AI::Outputs; ++o) {
		Neuron* neuron = network.neurons[AI::MaxNodes + o];
		assert(neuron);
		outputs[o] = neuron->value;
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

	assert(neurons.getSize());
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

	auto gene1 = gene;
	gene1.out = maxNeuron;
	gene1.weight = 1.f;
	gene1.innovation = pool->newInnovation();
	gene1.enabled = true;
	genes.push(gene1);

	auto gene2 = gene;
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

	Map<int, Gene*> innovations2;
	for (int i = 0; i < g2->genes.getSize(); ++i) {
		auto& gene = g2->genes[i];
		innovations2.insert(gene.innovation, &gene);
	}

	for (int i = 0; i < g1->genes.getSize(); ++i) {
		auto& gene1 = g1->genes[i];
		auto gene2 = innovations2[gene1.innovation];
		if (gene2 != nullptr && pool->rand.getUint8()%2 == 0 && (*gene2)->enabled) {
			child.genes.push(**gene2);
		} else {
			child.genes.push(gene1);
		}
	}

	child.maxNeuron = std::max(g1->maxNeuron, g2->maxNeuron);

	for (auto& pair : g1->mutationRates) {
		auto mutation = child.mutationRates[pair.a];
		assert(mutation);
		*mutation = pair.b;
	}

	return child;
}

float Species::disjoint(Genome* g1, Genome* g2) {
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
	return (float)result / n;
}

float Species::weights(Genome* g1, Genome* g2) {
	assert(g1);
	assert(g2);

	Map<int, Gene*> i2;
	for (int i = 0; i < g2->genes.getSize(); ++i) {
		auto& gene = g2->genes[i];
		i2.insert(gene.innovation, &gene);
	}

	float sum = 0;
	int coincident = 0;
	for (int i = 0; i < g1->genes.getSize(); ++i) {
		auto& gene = g1->genes[i];
		if (i2[gene.innovation] != nullptr) {
			auto& gene2 = **i2[gene.innovation];
			sum += fabs(gene.weight - gene2.weight);
			++coincident;
		}
	}

	return sum / coincident;
}

bool Species::sameSpecies(Genome* g1, Genome* g2) {
	assert(g1);
	assert(g2);

	float dd = AI::DeltaDisjoint * disjoint(g1, g2);
	float dw = AI::DeltaWeights * weights(g1, g2);
	return (dd + dw) < AI::DeltaThreshold;
}

void Species::calculateAverageFitness() {
	int64_t total = 0;

	for (int g = 0; g < genomes.getSize(); ++g) {
		auto& genome = genomes[g];
		total += genome.globalRank;
	}
	if (genomes.getSize()) {
		averageFitness = total / (int64_t)genomes.getSize();
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
			child = g;
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

int64_t Pool::totalAverageFitness() {
	int64_t total = 0;
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

		assert(spec.genomes.getSize());
		
		spec.genomes.sort(Genome::DescSort());

		if (spec.genomes[0].fitness > spec.topFitness) {
			spec.topFitness = spec.genomes[0].fitness;
			spec.staleness = 0;
		} else {
			++spec.staleness;
		}
		if (spec.staleness < AI::StaleSpecies || spec.topFitness >= maxFitness) {
			survived.push(spec);
		}
	}
	species.swap(survived);
}

void Pool::removeWeakSpecies() {
	ArrayList<Species> survived;
	int64_t sum = totalAverageFitness();
	for (int s = 0; s < species.getSize(); ++s) {
		auto& spec = species[s];
		int64_t breed = sum ? (int64_t)floorf(((float)spec.averageFitness / (float)sum) * (float)AI::Population) : 1;
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
	int64_t sum = totalAverageFitness();
	ArrayList<Genome> children;
	for (int s = 0; s < species.getSize(); ++s) {
		auto& spec = species[s];
		int64_t breed = (int64_t)floorf(((float)spec.averageFitness / (float)sum) * (float)AI::Population);
		for (int i = 0; i < breed; ++i) {
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
	int64_t maxFitnessInt = maxFitness.load();
	file->property("maxFitness", maxFitnessInt);
	maxFitness.store(maxFitnessInt);
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

AI::AI() {
}

AI::~AI() {
	if (pool) {
		delete pool;
		pool = nullptr;
	}
}

void AI::init() {
	if (pool) {
		delete pool;
		pool = nullptr;
	}
	pool = new Pool();
	pool->ai = this;
	pool->rand.seedTime();
	pool->inputSize = 16;
	pool->init();
	pool->writeFile("temp.json");
}

static const float aiClipNear = 10.f;
static const float aiClipFar = 500.f;

ArrayList<float> Genome::getInputs() {
	ArrayList<float> inputs;
	inputs.resize(pool->inputSize);
	assert(game.get());

	if (game->player) {
		float angle = 0.f;
		float finc = (PI * 2.f) / 16.f;
		for (int c = 0; c < 16; ++c) {
			auto& pos = game->player->pos;
			auto& ang = game->player->ang;
			auto result = game->player->rayTrace(pos, ang + angle);
			if (result.a) {
				//inputs[c * 2] = std::min(std::max((result.b - aiClipNear) / aiClipFar, 0.f), 1.f);
				//inputs[c * 2] = 1.f / result.b;
				inputs[c] = 1.f - std::min(std::max((result.b - aiClipNear) / aiClipFar, 0.f), 1.f);
				//Vector towardsPlayer(cos(ang + angle + PI), sin(ang + angle + PI), 0.f);
				//float redShift = result.a->vel.normal().dot(towardsPlayer);
				//inputs[c * 2 + 1] = redShift;
			} else {
				inputs[c] = 0.f;
				//inputs[c * 2] = 1.f;
				//inputs[c * 2 + 1] = 0.f;
			}
			angle += finc;
		}
		/*if (inputs[0] != 0.f && !game->player->shooting) {
			inputs[32] = 1.f;
		} else {
			inputs[32] = 0.f;
		}*/
		//inputs[32] = game->player->vel.x / 10.f;
		//inputs[33] = game->player->vel.y / 10.f;
	}

	return inputs;
}

void Genome::clearJoypad() {
	for (int c = 0; c < (int)Genome::Output::OUT_MAX; ++c) {
		outputs[c] = 0.f;
	}
}

void Genome::initializeRun() {
	game = std::make_shared<Game>(pool->ai, mainEngine->getXres(), mainEngine->getYres());
	game->genome = this;
	game->init();
	framesSurvived = 0;
	currentFrame = 0;
	finished = false;
	clearJoypad();
	generateNetwork();
}

void Genome::evaluateCurrent() {
	if (finished) {
		clearJoypad();
		return;
	}
	auto inputs = getInputs();
	auto controller = evaluateNetwork(inputs);

	if (controller.getSize()) {
		if (controller[Genome::Output::OUT_LEFT] && controller[Genome::Output::OUT_RIGHT]) {
			controller[Genome::Output::OUT_LEFT] = false;
			controller[Genome::Output::OUT_RIGHT] = false;
		}
		for (int c = 0; c < (int)Genome::Output::OUT_MAX; ++c) {
			outputs[c] = controller[c];
		}
	} else {
		for (int c = 0; c < (int)Genome::Output::OUT_MAX; ++c) {
			outputs[c] = 0.f;
		}
	}

	game->process();

	if (game->player) {
		int shotsFired = game->player->shotsFired;
		int shotsHit = game->player->shotsHit;
		framesSurvived = std::max(framesSurvived, (int)game->player->ticks);

		float greatestDanger = 0.f;
		for (int c = 0; c < 16; ++c) {
			greatestDanger = std::max(greatestDanger, inputs[c]);
		}
		totalDanger += 1.f - greatestDanger;

		fitness = game->score + 1;
		//fitness *= avgDistance / (framesSurvived + 1);
		fitness *= std::max(1.f, (float)framesSurvived);
		float dangerRate = totalDanger / framesSurvived;
		fitness *= dangerRate * dangerRate;
		//float hitRate = std::max((float)shotsHit, 1.f) / std::max((float)shotsFired, 1.f);
		//fitness *= hitRate * hitRate;
		if (fitness == 0) {
			fitness = -1;
		}
	}

	if (game->lives <= 2) {
		if (fitness > pool->maxFitness) {
			pool->maxFitness = fitness;
		}

		finished = true;
		game->term();
	}
	++currentFrame;
}

void AI::playTop() {
	int64_t maxFitness = 0;
	int maxs = 0, maxg = 0;
	focus = nullptr;
	for (int s = 0; s < pool->species.getSize(); ++s) {
		auto& spec = pool->species[s];
		for (int g = 0; g < spec.genomes.getSize(); ++g) {
			auto& genome = spec.genomes[g];
			if (!focus || (genome.game && genome.fitness > maxFitness)) {
				maxFitness = genome.fitness;
				focus = genome.game;
			}
		}
	}
}

int AI::getMeasured() const {
	int count = 0, done = 0;
	for (int s = 0; s < pool->species.getSize(); ++s) {
		auto& spec = pool->species[s];
		for (int g = 0; g < spec.genomes.getSize(); ++g) {
			auto& genome = spec.genomes[g];
			if (genome.finished) {
				++done;
			}
			++count;
		}
	}
	return (int)((float)done / (float)count * 100.f);
}

bool AI::process() {
	int threads = 0;
	bool result = true;

	if (mainEngine->pressKey(SDL_SCANCODE_F1)) {
		save();
	}
	if (mainEngine->pressKey(SDL_SCANCODE_F2)) {
		load();
	}

	std::vector<std::future<void>> tasks;

	int64_t maxFitness = 0;

	for (auto& spec : pool->species) {
		for (auto& gen : spec.genomes) {
			if (gen.game == nullptr) {
				gen.initializeRun();
			}
			if (threads < 150) {
				if (gen.finished) {
					if (gen.game == focus) {
						focus = nullptr;
					}
				} else {
					result = false;
					tasks.push_back(std::async(std::launch::async, &Genome::evaluateCurrent, &gen));
					++threads;
				}
			} else {
				if (!gen.finished) {
					result = false;
				}
			}
			if (!gen.finished) {
				if (!focus || !focus->gameInSession || (gen.game && gen.fitness > maxFitness)) {
					maxFitness = gen.fitness;
					focus = gen.game;
				}
			}
		}
	}

	for (auto& task : tasks) {
		task.wait();
	}

	return result;
}

void AI::save() {
	pool->savePool();
}

void AI::load() {
	pool->loadPool();
}

void AI::nextGeneration() {
	pool->rand.seedTime();
	pool->newGeneration();
}