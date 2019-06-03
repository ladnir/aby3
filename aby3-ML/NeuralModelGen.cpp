#include "NeuralModelGen.h"

void NeuralModelGen::sampleModel(int dim, int levels, int outDimension, oc::PRNG & prng)
{
	mModel.resize(levels);

	for (auto& m : mModel)
	{
		m.resize(dim, dim);
	}

}
