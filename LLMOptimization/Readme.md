# LLM Optimization Project

This project focuses on optimizing a Transformer-based language model using multi-threading techniques. The project includes the implementation of a quantized Transformer model, a tokenizer, and a sampler, along with multi-threaded matrix multiplication and multi-head attention mechanisms.

## Project Structure

- `common.h`: Contains common headers and definitions used across the project.
- `model.h`: Implements the Transformer model, tokenizer, and sampler.
- `seq.c`: Contains the single-threaded implementation of the model inference.
- `parallel_kohei0802.c`: Contains the multi-threaded implementation of the model inference.
- `Makefile`: Defines the build process for the project.

## Getting Started

### Prerequisites

- GCC compiler
- wget (for downloading model and tokenizer files)

### Building the Project

To build the project, run the following commands:

1. Prepare the environment by downloading the necessary model and tokenizer files:
    ```sh
    make prepare
    ```

2. Compile the single-threaded implementation:
    ```sh
    make seq
    ```

3. Compile the multi-threaded implementation:
    ```sh
    make parallel
    ```

### Running the Project

To run the single-threaded implementation, use the following command:
```sh
./seq <seed> <prompt>
```
Example:
```sh
./seq 42 "What is Fibonacci Number?"
```

To run the multi-threaded implementation, use the following command:
```sh
./parallel <num_threads> <seed> <prompt>
```
Example:
```sh
./parallel 4 42 "What is Fibonacci Number?"
```

### Cleaning Up

To clean the build artifacts, run:
```sh
make clean
```

To remove the downloaded model and tokenizer files, run:
```sh
make clean_bin
```

## Implementation Details

### Transformer Model

The Transformer model is implemented in `model.h` and includes:
- Quantized tensors for efficient storage and computation.
- Multi-head attention mechanism.
- Feed-forward neural network layers.
- Positional encoding using RoPE (Relative Positional Encoding).

### Tokenizer

The tokenizer is implemented in `model.h` and uses Byte Pair Encoding (BPE) to convert strings to tokens and vice versa.

### Sampler

The sampler is implemented in `model.h` and supports various sampling techniques, including:
- Greedy argmax sampling
- Multinomial sampling
- Top-p (nucleus) sampling

### Multi-threading

The multi-threaded implementation in `parallel_kohei0802.c` uses pthreads to parallelize the matrix multiplication and multi-head attention operations. The thread pool is managed using a custom thread pool implementation.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Acknowledgements

This project is modified from [llama2.c](https://github.com/karpathy/llama2.c) by Andrej Karpathy.