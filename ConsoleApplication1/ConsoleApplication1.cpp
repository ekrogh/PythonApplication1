// At the top of your file, with other includes
#include "rnnoise.h"
#include <iostream>
#include <vector>

// A simple function demonstrating rnnoise usage
void run_rnnoise_example()
{
    // Create a DenoiseState object. This holds all the state for the neural network.
    DenoiseState* st = rnnoise_create(NULL);

    // The audio frame must be exactly 480 samples of 16-bit PCM audio.
    const int FRAME_SIZE = 480;
    short pcm_in[FRAME_SIZE] = { 0 }; // Your noisy audio data goes here
    short pcm_out[FRAME_SIZE] = { 0 };

    // Convert short PCM to float for rnnoise
    std::vector<float> float_in(FRAME_SIZE);
    std::vector<float> float_out(FRAME_SIZE);

    for (int i = 0; i < FRAME_SIZE; ++i)
        float_in[i] = static_cast<float>(pcm_in[i]);

    // This is the core processing function. It applies the noise suppression.
    // It returns a VAD (Voice Activity Detection) probability.
    float vad_prob = rnnoise_process_frame(st, float_out.data(), float_in.data());

    // Convert float output back to short PCM
    for (int i = 0; i < FRAME_SIZE; ++i)
        pcm_out[i] = static_cast<short>(float_out[i]);

    std::cout << "Processed a frame of audio. VAD probability: " << vad_prob << std::endl;

    // When you're done, you must destroy the state to free memory.
    rnnoise_destroy(st);
}

int main()
{
    // You can call the example function from main()
    run_rnnoise_example();

    std::cout << "Calling Python to find the sum of 2 and 2.\n";

    // ... (rest of your main function)
}