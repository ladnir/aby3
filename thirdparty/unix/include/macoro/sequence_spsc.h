///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include "macoro/config.h"
#include "macoro/sequence_barrier.h"
#include "macoro/sequence_range.h"

namespace macoro
{
	template<typename SEQUENCE, typename TRAITS>
	class sequence_spsc_claim_one_operation;

	template<typename SEQUENCE, typename TRAITS>
	class sequence_spsc_claim_operation;

	template<
		typename SEQUENCE = std::size_t,
		typename TRAITS = sequence_traits<SEQUENCE>>
	class sequence_spsc
	{
	public:
		static constexpr bool multi_sender = false;
		using size_type = typename sequence_range<SEQUENCE, TRAITS>::size_type;

		sequence_spsc(
			const sequence_barrier<SEQUENCE, TRAITS>& consumerBarrier,
			std::size_t bufferSize,
			SEQUENCE initialSequence = TRAITS::initial_sequence) noexcept
			: m_consumerBarrier(consumerBarrier)
			, m_bufferSize(bufferSize)
			, m_nextToClaim(initialSequence + 1)
			, m_producerBarrier(initialSequence)
		{}

		/// Claim a slot in the ring buffer asynchronously.
		///
		/// \return
		/// Returns an operation that when awaited will suspend the coroutine until
		/// a slot is available for writing in the ring buffer. The result of the
		/// co_await expression will be the sequence number of the slot.
		/// The caller must publish() the claimed sequence number once they have written to
		/// the ring-buffer.
		[[nodiscard]]
		sequence_spsc_claim_one_operation<SEQUENCE, TRAITS>
		claim_one() noexcept;

		/// Claim one or more contiguous slots in the ring-buffer.
		///
		/// Use this method over many calls to claim_one() when you have multiple elements to
		/// enqueue. This will claim as many slots as are available up to the specified count
		/// but may claim as few as one slot if only one slot is available.
		///
		/// \param count
		/// The maximum number of slots to claim.
		///
		/// \return
		/// Returns an awaitable object that when awaited returns a sequence_range that contains
		/// the range of sequence numbers that were claimed. Once you have written element values
		/// to all of the claimed slots you must publish() the sequence range in order to make
		/// the elements available to consumers.
		[[nodiscard]]
		sequence_spsc_claim_operation<SEQUENCE, TRAITS> claim_up_to(
			std::size_t count) noexcept;

		/// Publish the specified sequence number.
		///
		/// This also implies that all prior sequence numbers have already been published.
		void publish(SEQUENCE sequence) noexcept
		{
			m_producerBarrier.publish(sequence);
		}

		/// Publish a contiguous range of sequence numbers.
		///
		/// You must have already published all prior sequence numbers.
		///
		/// This is equivalent to just publishing the last sequence number in the range.
		void publish(const sequence_range<SEQUENCE, TRAITS>& sequences) noexcept
		{
			m_producerBarrier.publish(sequences.back());
		}

		/// Query what the last-published sequence number is.
		///
		/// You can assume that all prior sequence numbers are also published.
		SEQUENCE last_published() const noexcept
		{
			return m_producerBarrier.last_published();
		}

		/// Asynchronously wait until the specified sequence number is published.
		///
		/// \param targetSequence
		/// The sequence number to wait for.
		///
		/// \return
		/// Returns an Awaitable type that, when awaited, will suspend the awaiting coroutine until the
		/// specified sequence number has been published.
		///
		/// The result of the 'co_await barrier.wait_until_published(seq)' expression will be the
		/// last-published sequence number, which is guaranteed to be at least 'seq' but may be some
		/// subsequent sequence number if additional items were published while waiting for the
		/// the requested sequence number to be published.
		[[nodiscard]]
		auto wait_until_published(SEQUENCE targetSequence) const noexcept
		{
			return m_producerBarrier.wait_until_published(targetSequence);
		}

		[[nodiscard]]
		auto wait_until_published(SEQUENCE targetSequence, SEQUENCE lastKnown) const noexcept
		{
			return wait_until_published(targetSequence);
		}


	private:

		template<typename SEQUENCE2, typename TRAITS2>
		friend class sequence_spsc_claim_operation;

		template<typename SEQUENCE2, typename TRAITS2>
		friend class sequence_spsc_claim_one_awaiter;

#if _MSC_VER
# pragma warning(push)
# pragma warning(disable : 4324) // C4324: structure was padded due to alignment specifier
#endif

		const sequence_barrier<SEQUENCE, TRAITS>& m_consumerBarrier;
		const std::size_t m_bufferSize;

		alignas(MACORO_CPU_CACHE_LINE)
		SEQUENCE m_nextToClaim;

		sequence_barrier<SEQUENCE, TRAITS> m_producerBarrier;

#if _MSC_VER
# pragma warning(pop)
#endif
	};


	template<typename SEQUENCE, typename TRAITS>
	class sequence_spsc_claim_one_awaiter
	{
	public:

		sequence_spsc_claim_one_awaiter(
			sequence_spsc<SEQUENCE, TRAITS>& sequencer) noexcept
			: m_consumerWaitOperation(
				sequencer.m_consumerBarrier,
				static_cast<SEQUENCE>(sequencer.m_nextToClaim - sequencer.m_bufferSize))
			, m_sequencer(sequencer)
		{}


		bool await_ready() const noexcept
		{
			return m_consumerWaitOperation.await_ready();
		}

#ifdef MACORO_CPP_20
		auto await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept
		{
			return await_suspend(coroutine_handle<>(awaitingCoroutine));
		}
#endif
		auto await_suspend(coroutine_handle<> awaitingCoroutine) noexcept
		{
			return m_consumerWaitOperation.await_suspend(awaitingCoroutine);
		}

		SEQUENCE await_resume() const noexcept
		{
			return m_sequencer.m_nextToClaim++;
		}

	private:

		sequence_barrier_wait_operation_base<SEQUENCE, TRAITS> m_consumerWaitOperation;
		sequence_spsc<SEQUENCE, TRAITS>& m_sequencer;

	};

	template<typename SEQUENCE, typename TRAITS>
	class sequence_spsc_claim_one_operation
	{
	public:

		sequence_spsc_claim_one_operation(
			sequence_spsc<SEQUENCE, TRAITS>& sequencer) noexcept
			: m_sequencer(sequencer)
		{}


		sequence_spsc_claim_one_awaiter<SEQUENCE,TRAITS> MACORO_OPERATOR_COAWAIT() noexcept
		{
			return m_sequencer;
		}

	private:
		sequence_spsc<SEQUENCE, TRAITS>& m_sequencer;
	};

	template<typename SEQUENCE, typename TRAITS>
	class sequence_spsc_claim_operation
	{
	public:

		explicit sequence_spsc_claim_operation(
			sequence_spsc<SEQUENCE, TRAITS>& sequencer,
			std::size_t count) noexcept
			: m_consumerWaitOperation(
				sequencer.m_consumerBarrier,
				static_cast<SEQUENCE>(sequencer.m_nextToClaim - sequencer.m_bufferSize))
			, m_sequencer(sequencer)
			, m_count(count)
		{}

		bool await_ready() const noexcept
		{
			return m_consumerWaitOperation.await_ready();
		}

#ifdef MACORO_CPP_20
		auto await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept
		{
			return await_suspend(coroutine_handle<>(awaitingCoroutine));
		}
#endif
		auto await_suspend(coroutine_handle<> awaitingCoroutine) noexcept
		{
			return m_consumerWaitOperation.await_suspend(awaitingCoroutine);
		}

		sequence_range<SEQUENCE, TRAITS> await_resume() noexcept
		{
			const SEQUENCE lastAvailableSequence =
				static_cast<SEQUENCE>(m_consumerWaitOperation.await_resume() + m_sequencer.m_bufferSize);
			const SEQUENCE begin = m_sequencer.m_nextToClaim;
			const std::size_t availableCount = static_cast<std::size_t>(lastAvailableSequence - begin) + 1;
			const std::size_t countToClaim = std::min(m_count, availableCount);
			const SEQUENCE end = static_cast<SEQUENCE>(begin + countToClaim);
			m_sequencer.m_nextToClaim = end;
			return sequence_range<SEQUENCE, TRAITS>(begin, end);
		}

	private:

		sequence_barrier_wait_operation_base<SEQUENCE, TRAITS> m_consumerWaitOperation;
		sequence_spsc<SEQUENCE, TRAITS>& m_sequencer;
		std::size_t m_count;

	};

	template<typename SEQUENCE, typename TRAITS>
	[[nodiscard]]
	sequence_spsc_claim_one_operation<SEQUENCE, TRAITS>
	sequence_spsc<SEQUENCE, TRAITS>::claim_one() noexcept
	{
		return sequence_spsc_claim_one_operation<SEQUENCE, TRAITS>{ *this };
	}

	template<typename SEQUENCE, typename TRAITS>
	[[nodiscard]]
	sequence_spsc_claim_operation<SEQUENCE, TRAITS>
	sequence_spsc<SEQUENCE, TRAITS>::claim_up_to(std::size_t count) noexcept
	{
		return sequence_spsc_claim_operation<SEQUENCE, TRAITS>(*this, count);
	}
}

