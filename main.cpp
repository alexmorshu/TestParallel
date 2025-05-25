#include <atomic>
#include <thread>
#include <vector>



class Barrier final
{
public: 
	Barrier(const std::size_t max) noexcept: count_(0), max_(max)
	{}
	void increaseWait() noexcept
	{
		increase_();
		wait_();
	}
	void clear() noexcept
	{
		count_.store(0, std::memory_order_relaxed);
	}
private:
	void wait_() const noexcept
	{
		std::size_t count;
		do
		{
			count = count_.load(std::memory_order_acquire);
		}
		while(count != max_);
	}
	void increase_() noexcept
	{
		count_.fetch_add(1, std::memory_order_release);
	}

private:
	std::atomic<std::size_t> count_;
	std::size_t max_;
};


template<class Data>
class TestParallel
{
public:
	TestParallel(
			void (*Action)(Data&) noexcept, bool (*CheckData)(const Data&) noexcept
			, void (*ResetData)(Data&) noexcept, std::size_t count = 100'000
		    ):countCores_(std::thread::hardware_concurrency()), b1(countCores_), b2(countCores_), b3(countCores_)
		      	, count_(count)
	{
		threads_.reserve(countCores_);
		for(unsigned int i = 0; i < countCores_; i++)
			threads_.push_back(std::thread(TestParallel<Data>::worker_, this, Action, CheckData, ResetData));
	}

	void wait()
	{
		for(std::thread& thread : threads_)
		{
			thread.join();
		}
	}

	bool isSuccess()
	{
		return !(isStopped_.load(std::memory_order_relaxed));
	}

private:	
	static void worker_(TestParallel* p,
			void (*Action)(Data&) noexcept, bool (*CheckData)(const Data&) noexcept
			,void (*ResetData)(Data&) noexcept
		)
	{
		for(std::size_t i = 0; i < p->count_; i++)
		{
			if(p->isStopped_)
			{
				break;
			}
			p->b1.increaseWait();
			Action(p->data_);
			p->b3.clear();
			p->b2.increaseWait();
			if(!CheckData(p->data_))
			{
				p->isStopped_.store(true, std::memory_order_relaxed);
			}
			p->b1.clear();
			p->b3.increaseWait();
			ResetData(p->data_);
			p->b2.clear();
		}
	}
private:
	unsigned int countCores_;
	Barrier b1;
	Barrier b2;
	Barrier b3;
	std::size_t count_;
	std::vector<std::thread> threads_;
	Data data_;
	std::atomic<bool> isStopped_ = false;
};

struct Data
{
	int i = 0;
	std::atomic<int> realI = 0;
};

void action(Data& data) noexcept
{
	data.i++;
	data.realI++;
}

bool check(const Data& data) noexcept
{
	return data.i == data.realI;	
}
void reset(Data& data) noexcept
{
	data.i = 0;
	data.realI = 0;
}

int main()
{
	TestParallel<Data> test(action, check, reset, 1000);
	test.wait();
	std::cout << test.isSuccess() << std::endl;
}
