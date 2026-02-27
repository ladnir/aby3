#pragma once
#include "cryptoTools/Common/Defines.h"
#ifdef ENABLE_CIRCUITS

#include "MxBit.h"
#include <unordered_map>
#include "macoro/optional.h"
#include "macoro/variant.h"
#include "cryptoTools/Circuit/BetaCircuit.h"
#include<numeric>
namespace osuCrypto
{
	namespace Mx
	{

		template <typename... Fs>
		struct match : Fs... {
			using Fs::operator()...;
			// constexpr match(Fs &&... fs) : Fs{fs}... {}
		};
		template<class... Ts> match(Ts...) -> match<Ts...>;

		template <typename Var, typename... Fs>
		constexpr decltype(auto) operator| (Var&& v, match<Fs...> const& match) {
			return std::visit(match, v);
		}



		//class GraphCircuit
		//{
		//public:
		//	struct InputNode
		//	{
		//		//InputNode() noexcept = default;
		//		//InputNode(InputNode&&) noexcept = default;
		//		//InputNode& operator=(InputNode&&) noexcept = default;
		//		//InputNode(u64 i) noexcept : mIndex(i) {}

		//		u64 mIndex = 0;
		//	};

		//	struct OpNode
		//	{
		//		//OpNode() noexcept = default;
		//		//OpNode(OpNode&&) noexcept = default;
		//		//OpNode& operator=(OpNode&&) noexcept = default;
		//		//OpNode(OpType i) noexcept : mType(i) {}

		//		OpType mType;
		//	};

		//	struct PrintNode
		//	{

		//		using Fn = std::function<std::string(const BitVector& b)>;
		//		//PrintNode() noexcept = default;
		//		//PrintNode(PrintNode&&) noexcept = default;
		//		//PrintNode& operator=(PrintNode&&) noexcept = default;
		//		//PrintNode(Fn& f) : mFn(f) {}

		//		Fn* mFn;
		//	};

		//	struct OutputNode
		//	{

		//		//OutputNode() noexcept = default;
		//		//OutputNode(OutputNode&&) noexcept = default;
		//		//OutputNode& operator=(OutputNode&&) noexcept = default;
		//		//OutputNode(u64 i) noexcept : mIndex(i) {}

		//		u64 mIndex = 0;
		//	};

		//	struct Node
		//	{
		//		Node()
		//			: mData(InputNode{})
		//		{}

		//		//u64 level = 0;
		//		SmallVector<u64, 2> mInputs;
		//		SmallSet<u64, 2> mDeps;
		//		SmallVector<u64, 1> mOutputs;
		//		SmallSet<u64, 1> mChildren;
		//		macoro::variant<InputNode, OpNode, PrintNode, OutputNode> mData;

		//		bool isLinear()
		//		{
		//			return mData | match{
		//				[](InputNode&) {return true; },
		//				[](OpNode& o) { return Mx::isLinear(o.mType); },
		//				[&](PrintNode&) {return mInputs.size() == 0; },
		//				[](OutputNode&) {return true; }
		//			};
		//		}

		//		void addDep(u64 d)
		//		{
		//			//if (std::find(mDeps.begin(), mDeps.end(), d) == mDeps.end())
		//			mDeps.insert(d);
		//		}

		//		void addChild(u64 d)
		//		{
		//			mChildren.insert(d);
		//			//if (std::find(mChildren.begin(), mChildren.end(), d) == mChildren.end())
		//		}

		//		void removeDep(u64 d)
		//		{
		//			mDeps.erase(d);
		//			//auto iter = mDeps.find(d);

		//			//auto iter = std::find(mDeps.begin(), mDeps.end(), d);
		//			//if (iter == mDeps.end())
		//			//	throw RTE_LOC;
		//			//std::swap(*iter, mDeps.back());
		//			//mDeps.pop_back();
		//		}
		//	};

		//	std::vector<Node> mNodes;
		//	std::vector<u64>mInputs, mOutputs;
		//};

		class Circuit
		{
		public:

			Circuit() = default;
			Circuit(Circuit&&) = delete;
			Circuit(const Circuit&) = delete;

			enum ValueType
			{
				Binary,
				Z2k
			};

			struct OpData
			{
				virtual ~OpData() {}
			};

			template<typename T>
			struct Arena
			{
				struct Alloc
				{
					std::unique_ptr<T[]> mPtr;
					T* mBegin,* mEnd;
					//span<T> mFree;
				};

				std::vector<Alloc> mAllocs;
				std::vector<u64> mFreeList;
				u64 mTotal = 0;

				void reserve(u64 n)
				{
					mAllocs.emplace_back();
					mAllocs.back().mPtr.reset(new T[n]);
					mAllocs.back().mBegin = mAllocs.back().mPtr.get();
					mAllocs.back().mEnd = mAllocs.back().mPtr.get() + n;
					mFreeList.push_back(mAllocs.size() - 1);
					mTotal += n;
				}

				span<T> allocate(u64 n)
				{
					for (u64 i = mFreeList.size()-1; i < mFreeList.size(); --i)
					{
						auto& f = mAllocs[mFreeList[i]];
						if (f.mBegin + n <= f.mEnd)
						{
							auto ret = span<T>(f.mBegin, n);
							assert(ret.data() + ret.size() <= f.mEnd);
							f.mBegin += n;
							if (f.mBegin == f.mEnd)
							{
								std::rotate(
									mFreeList.begin() + i,
									mFreeList.begin() + i + 1,
									mFreeList.end());

								mFreeList.pop_back();
							}
							return ret;
						}
					}

					auto newSize = std::max<u64>({ 64, mTotal, n });
					reserve(newSize);
					return allocate(n);
				}
			};


			struct Gate
			{
				// the address of the gate/offset that we consume.
				span<Address> mInput;

				// the number of outputs this gate produces.
				u64 mNumOutputs = 0;

				// the type of this gate.
				OpType mType;

				// axilary data.
				std::unique_ptr<OpData> mData;


				bool isLinear() const
				{
					if (mType == OpType::Print)
						return mInput.size() == 0;
					else
						return Mx::isLinear(mType);
				}

			};


			struct Input : OpData
			{
				u64 mIndex;
			};

			struct Output :OpData
			{
				u64 mIndex;
				std::vector<macoro::optional<bool>> mConsts;
			};

			struct Print : OpData
			{
				Print() = default;
				Print(const Print&) = default;
				Print(Print&&) = default;
				Print& operator=(const Print&) = default;
				Print& operator=(Print&&) = default;

				Print(std::function<std::string(const BitVector& b)>&& f)
					: mFn(std::move(f))
				{}

				std::function<std::string(const BitVector& b)> mFn;
			};

			std::vector<u64> mInputs, mOutputs;

			template <typename T, typename ... Args>
			T input(Args...);

			template <typename T>
			void output(T&);

			//u64 mNextBitIdx = 0;
			//u64 addBitMap(Bit& b) {
			//	//auto iter = mBitMap.find(&b);
			//	if (b.mAddress == ~0ull)
			//	{
			//		auto idx = mNextBitIdx++;
			//		//mBitMap.insert({ &b, idx });
			//		b.mCir = this;
			//		b.mAddress = idx;
			//		return idx;
			//	}
			//	else
			//		throw std::runtime_error("internal error: bit has already been mapped. " LOCATION);
			//}

			Address getBitMap(const Bit& b)
			{
				if (b.mAddress.hasValue() == false)
					throw std::runtime_error("error: reading an uninitilized value. " LOCATION);
				auto addr = b.mAddress.gate();
				auto off = b.mAddress.offset();
				assert(addr < mGates.size() &&
					off < mGates[addr].mNumOutputs);

				return b.mAddress;
			}

			Arena<Address> mArena;
			std::vector<Gate> mGates;
			u64 mPrevPrintIdx = ~0ull;

			void addPrint(span<const Bit*> elems,
				std::function<std::string(const BitVector& b)>&& p)
			{
				Gate g;
				g.mType = OpType::Print;

				u64 s = (mPrevPrintIdx != ~0ull);
				for (u64 i = 0; i < elems.size(); ++i)
					s += elems[i]->isConst() == false;

				g.mInput = mArena.allocate(s);

				std::vector<macoro::optional<bool>> consts(elems.size());
				u64 j = 0;
				for (u64 i = 0; i < elems.size(); ++i)
				{
					if (elems[i]->isConst() == false)
						g.mInput[j++] = getBitMap(*elems[i]);
					else
						consts[i] = elems[i]->constValue();
				}

				if (mPrevPrintIdx != ~0ull)
				{
					g.mInput[j++] = Address(mPrevPrintIdx, ~0ull);
				}

				assert(j == s);

				g.mData = std::make_unique<Print>(
					[consts = std::move(consts), f = std::move(p)](const BitVector& bv) -> std::string
					{
						BitVector v; v.reserve(consts.size());

						for (u64 i = 0, j = 0; i < consts.size(); ++i)
							if (consts[i].has_value() == false)
								v.pushBack(bv[j++]);
							else
								v.pushBack(*consts[i]);

						return f(v);
					});

				mPrevPrintIdx = mGates.size();
				mGates.push_back(std::move(g));

			}

			Bit addGate(OpType t, const Bit& a, const Bit& b)
			{
				Bit ret;
				ret.mCir = this;
				ret.mAddress = Address(mGates.size(), 0);
				mGates.emplace_back();
				auto& g = mGates.back();

				g.mInput = mArena.allocate(2);
				g.mInput[0] = getBitMap(a);
				g.mInput[1] = getBitMap(b);
				g.mNumOutputs = 1;
				g.mType = t;

				return ret;
			}

			//void copy(const Bit& a, Bit& d)
			//{
			//	Bit ret;
			//	ret.mCir = this;
			//	ret.mAddress = Address(mGates.size(), 0);
			//	mGates.emplace_back();
			//	auto& g = mGates.back();

			//	g.mInput.push_back(getBitMap(a));
			//	g.mOutput.push_back(getBitMap(d));
			//	g.mType = OpType::a;
			//	mGates.push_back(std::move(g));
			//}

			//void move(Bit&& a, Bit& d)
			//{
			//	//auto iter = mBitMap.find(&a);
			//	if (a.mAddress == ~0ull)
			//		throw std::runtime_error("uninitialized bit was moved. " LOCATION);

			//	if (d.mAddress != ~0ull)
			//		remove(d);

			//	//mBitMap[&d] = idx;
			//	//auto idx = a.mAddress;

			//	d.mCir = this;
			//	d.mAddress = a.mAddress;
			//	a.mAddress = ~0ull;
			//	//mBitMap.erase(iter);
			//}

			//void remove(Bit& a)
			//{
			//	//auto iter = mBitMap.find(&a);
			//	if (a.mAddress == ~0ull)
			//		throw std::runtime_error("uninitialized bit was removed. " LOCATION);
			//	a.mAddress = ~0ull;
			//	//mBitMap.erase(iter);
			//}

			Bit negate(const Bit& a)
			{
				Bit ret;
				ret.mCir = this;
				ret.mAddress = Address(mGates.size(), 0);
				mGates.emplace_back();
				auto& g = mGates.back();

				g.mInput = mArena.allocate(1);
				g.mInput[0] = getBitMap(a);
				g.mNumOutputs = 1;
				g.mType = OpType::na;

				return ret;
			}


			void evaluate(const std::vector<BitVector>& in, std::vector<BitVector>& out)
			{
				if (in.size() != mInputs.size())
					throw std::runtime_error("MxCircuit::evaluate(...), number of inputs provided is not correct. " LOCATION);
				out.resize(mOutputs.size());

				std::vector<u64> map(mGates.size(), ~0ull);
				u64 nc = std::accumulate(mGates.begin(), mGates.end(), 0ull, [](auto&& c, auto&& g) {
					return g.mNumOutputs + c;
					});
				std::unique_ptr<u8[]>vals_(new u8[nc]);
				auto vals = [&](const Address& a) -> auto& {
					assert(map[a.gate()] != ~0ull);
					return vals_[map[a.gate()] + a.offset()];
					};


				for (u64 i = 0, j = 0; i < mGates.size(); ++i)
				{
					auto& gate = mGates[i];
					map[i] = j; j += gate.mNumOutputs;
					switch (gate.mType)
					{
					case OpType::a:
						vals(Address(i, 0)) = vals(gate.mInput[0]);
						break;
					case OpType::na:
						vals(Address(i, 0)) = !vals(gate.mInput[0]);
						break;
					case OpType::And:
						vals(Address(i, 0)) = vals(gate.mInput[0]) & vals(gate.mInput[1]);
						break;
					case OpType::Or:
						vals(Address(i, 0)) = vals(gate.mInput[0]) | vals(gate.mInput[1]);
						break;
					case OpType::Xor:
						vals(Address(i, 0)) = vals(gate.mInput[0]) ^ vals(gate.mInput[1]);
						break;
					case OpType::Input:
					{
						auto d = dynamic_cast<Input*>(gate.mData.get());
						assert(d);
						auto idx = d->mIndex;
						auto& x = in[idx];
						if (x.size() != gate.mNumOutputs)
							throw std::runtime_error("MxCircuit::evaluate(...), the " + std::to_string(idx) + "'th input provided is not the correct size. " LOCATION);

						for (u64 j = 0; j < x.size(); ++j)
						{
							vals(Address(i, j)) = x[j];
						}
						break;
					}
					case OpType::Output:
					{
						auto d = dynamic_cast<Output*>(gate.mData.get());
						assert(d);
						auto idx = d->mIndex;
						assert(idx < mOutputs.size());
						out[idx].resize(d->mConsts.size());

						for (u64 j = 0, k = 0; j < d->mConsts.size(); ++j)
						{
							if (d->mConsts[j].has_value())
								out[idx][j] = d->mConsts[j].value();
							else
								out[idx][j] = vals(gate.mInput[k++]);
						}

						break;
					}
					case OpType::Print:
					{

						Print* p = dynamic_cast<Print*>(gate.mData.get());

						auto s = gate.mInput.size();
						if (s && gate.mInput.back().offset() == ~0ull)
							--s;
						BitVector v(s);
						for (u64 j = 0; j < v.size(); ++j)
						{
							v[j] = vals(gate.mInput[j]);
						}
						std::cout << p->mFn(v);
						break;
					}
					default:
						throw std::runtime_error("MxCircuit::evaluate(...), gate type not implemented. " LOCATION);
					}
				}


				//for (u64 i = 0; i < out.size(); ++i)
				//{
				//}
			}

			struct ChildEdges
			{
				// the address of the gate/offset that consumes this gate.
				// these will be computed once the circuit is done.
				std::vector<span<Address>> mChildren;

				Circuit::Arena<Address> mChildArena;


			};

			ChildEdges computeChildEdges();

			oc::BetaCircuit asBetaCircuit();
			//GraphCircuit asGraph() ;
		};

		inline std::ostream& operator<<(std::ostream& o, const Circuit& c)
		{
			u64 j = 0;
			for (auto& g : c.mGates)
			{
				o << j << ": " << g.mType << " {";
				for (auto i : g.mInput)
				{
					o << " " << i;
				}
				o << " } -> { ";

				for (u64 i = 0; i < g.mNumOutputs; ++i)
				{
					o << " " << Address(j, i);
				}

				o << " }\n";
				++j;
			}
			return o;
		}

		template<typename T, typename = void>
		struct has_bit_representation_type : std::false_type
		{};

		template <typename T>
		struct has_bit_representation_type <T, std::void_t<
			// must have representation_type
			typename std::remove_reference_t<T>::representation_type,

			// must be Bit
			std::enable_if_t<
			std::is_same<
			typename std::remove_reference_t<T>::representation_type,
			Bit
			>::value
			>
			>>
			: std::true_type{};


		Circuit& operator<<(Circuit& o, span<const Bit> bits);
		inline Circuit& operator<<(Circuit& o, span<Bit> bits)
		{
			span<const Bit> b2(bits);
			return o << b2;
		}

		template<typename T>
		inline typename std::enable_if<
			has_bit_representation_type<T>::value == true
			, Circuit&>::type operator<<(Circuit& o, T&& t)
		{
			auto elems = t.serialize();
			o.addPrint(elems, t.toString());
			return o;
		}



		template<typename T>
		inline typename std::enable_if<
			has_bit_representation_type<T>::value == false
			, Circuit&>::type operator<<(Circuit& o, T&& t)
		{
			std::stringstream ss;
			ss << t;
			o.addPrint({}, [str = ss.str()](const BitVector&) ->std::string { return str; });
			return o;
		}

		template<typename T, typename ...Args>
		inline T Circuit::input(Args ... args)
		{
			u64 idx = mGates.size();
			mInputs.reserve(10);
			mInputs.push_back(idx);
			mGates.reserve(1000);
			mGates.emplace_back();
			Gate& g = mGates.back();

			T input(std::forward<Args>(args)...);

			auto elems = input.deserialize();

			g.mType = OpType::Input;
			g.mNumOutputs = elems.size();
			auto in = std::make_unique<Input>();
			in->mIndex = mInputs.size() - 1;
			g.mData = std::move(in);

			u64 j = 0;
			for (auto& e : elems)
			{
				e->mCir = this;
				e->mAddress = Address(idx, j++);
				//g.mOutput.push_back(addBitMap(*e));
			}

			return input;
		}

		template<typename T>
		inline void Circuit::output(T& o)
		{
			u64 idx = mGates.size();
			mOutputs.reserve(10);
			mOutputs.push_back(idx);
			mGates.emplace_back();
			Gate& g = mGates.back();

			auto elems = o.serialize();
			g.mType = OpType::Output;

			u64 s = 0;
			for (u64 i = 0; i < elems.size(); ++i)
				s += elems[i]->isConst() == false;

			auto out = std::make_unique<Output>();
			out->mConsts.reserve(elems.size());
			out->mIndex = mOutputs.size() - 1;
			g.mInput = mArena.allocate(s);

			u64  j = 0;
			for (auto& e : elems)
			{
				if (e->isConst())
				{
					out->mConsts.emplace_back(e->constValue());
				}
				else
				{
					out->mConsts.emplace_back();
					g.mInput[j++] = e->mAddress;
				}
			}
			assert(j == s);

			g.mData = std::move(out);
		}


	}
}
#endif
