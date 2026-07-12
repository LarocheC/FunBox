// fourier.h
//
// Vendored from GuitarML/Funbox  (software/Venus/fourier.h), which in turn
// adapts amcerbu/DaisySTFT.  The ONLY change from the original is that the
// per-window `processor` callback is stored as a std::function instead of a
// raw function pointer, so that the Venus plugin engine can bind a stateful
// member function (`VenusEngine::reverbProcess`).  This lets several plugin
// instances run at once without sharing global state.  All STFT math is
// unchanged.
#ifndef FOURIER

#include <functional>
#include <cstring>

#include "wave.h"

namespace soundmath
{
	template <typename T, size_t N> class Fourier
	{
	public:
		std::function<void(const T* in, T* out)> processor;

		// in, middle, out need to be arrays of size (N * laps * 2)
		Fourier(std::function<void(const T*, T*)> processor, ShyFFT<T, N, RotationPhasor>* fft, Wave<T>* window, size_t laps, T* in, T* middle, T* out)
			: processor(processor), in(in), middle(middle), out(out), fft(fft), window(window), laps(laps), stride(N / laps)
		{
			writepoints = new int[laps * 2];
			readpoints = new int[laps * 2];

			memset(writepoints, 0, sizeof(int) * laps * 2);
			memset(readpoints, 0, sizeof(int) * laps * 2);

			for (int i = 0; i < 2 * (int)laps; i++) // initialize half of writepoints
				writepoints[i] = -i * (int)stride;

			reading = new bool[laps * 2];
			writing = new bool[laps * 2];

			memset(reading, false, sizeof(bool) * laps * 2);
			memset(writing, true, sizeof(bool) * laps * 2);
		}

		~Fourier()
		{
			delete [] writepoints;
			delete [] readpoints;
			delete [] reading;
			delete [] writing;
		}

		// writes a single sample (with windowing) into the in array
		void write(T x)
		{
			for (size_t i = 0; i < laps * 2; i++)
			{
				if (writing[i])
				{
					if (writepoints[i] >= 0)
					{
						T amp = (*window)((T)writepoints[i] / N);
						in[writepoints[i] + N * i] = amp * x;
					}
					writepoints[i]++;

					if (writepoints[i] == N)
					{
						writing[i] = false;
						reading[i] = true;
						readpoints[i] = 0;

						forward(i); // FTs ith in to ith middle buffer
						process(i); // user-defined; ought to move info from ith middle to out buffer
						backward(i); // IFTs ith out to ith in buffer

						current = i;
					}
				}
			}
		}

		inline void forward(const size_t i)
		{
			fft->Direct((in + i * N), (middle + i * N)); // analysis
		}

		inline void backward(const size_t i)
		{
			fft->Inverse((out + i * N), (in + i * N)); // synthesis
		}

		// executes user-defined callback
		inline void process(const size_t i)
		{
			processor((middle + i * N), (out + i * N));
		}

		// read a single reconstructed sample
		T read()
		{
			T accum = 0;

			for (size_t i = 0; i < laps * 2; i++)
			{
				if (reading[i])
				{
					T amp = (*window)((T)readpoints[i] / N);
					accum += amp * in[readpoints[i] + N * i];

					readpoints[i]++;

					if (readpoints[i] == N)
					{
						writing[i] = true;
						reading[i] = false;
						writepoints[i] = 0;
					}
				}
			}

			accum /= N * laps / 2.0;
			return accum;
		}



	private:
		T *in, *middle, *out;

	public:
		ShyFFT<T, N, RotationPhasor>* fft;
		Wave<T>* window;

		size_t laps;
		size_t stride;

		int* writepoints;
		int* readpoints;
		bool* reading;
		bool* writing;

		int current = 0;
	};
}

#define FOURIER
#endif
