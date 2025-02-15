#pragma once

/// @defgroup TaskFSM Task FSM
/// @brief Finite state machine that implements states using task factories
/// @{
/// 
/// Full documentation of the TaskFSM system coming soon!

#include "Task.h"

NAMESPACE_SQUID_BEGIN

class TaskFSM;

namespace FSM
{
// State ID wrapper
struct StateId
{
	StateId() = default;
	StateId(int32_t in_idx) : idx(in_idx) {}
	StateId(size_t in_idx) : idx((int32_t)in_idx) {}
	bool operator==(const StateId& other) const { return (other.idx == idx); }
	bool operator!=(const StateId& other) const { return (other.idx != idx); }
	bool IsValid() const { return idx != INT32_MAX; }

	int32_t idx = INT32_MAX; // Default to invalid idx
};

// State transition debug data
struct TransitionDebugData
{
	FSM::StateId oldStateId;
	FString oldStateName;
	FSM::StateId newStateId;
	FString newStateName;
};

// State transition callback function
using tOnStateTransitionFn = TFunction<void()>;

#include "Private/TaskFSMPrivate.h" // Internal use only! Do not include elsewhere!

//--- State Handle ---//
template<class tStateInput, class tStateConstructorFn>
class StateHandle
{
	using tPredicateRet = typename std::conditional<!std::is_void<tStateInput>::value, TOptional<tStateInput>, bool>::type;
	using tPredicateFn = TFunction<tPredicateRet()>;
public:
	StateHandle(StateHandle&& in_other) = default;
	StateHandle& operator=(StateHandle&& in_other) = default;

	StateId GetId() const //< Get the ID of this state
	{
		return m_state ? m_state->idx : StateId{};
	}

// SFINAE Template Declaration Macros (#defines)
/// @cond
#define NONVOID_ONLY_WITH_PREDICATE template <class tPredicateFn, typename tPayload = tStateInput, typename std::enable_if_t<!std::is_void<tPayload>::value>* = nullptr>
#define VOID_ONLY_WITH_PREDICATE template <class tPredicateFn, typename tPayload = tStateInput, typename std::enable_if_t<std::is_void<tPayload>::value>* = nullptr>
#define NONVOID_ONLY template <typename tPayload = tStateInput, typename std::enable_if_t<!std::is_void<tPayload>::value && !std::is_convertible<tPayload, tPredicateFn>::value>* = nullptr>
#define VOID_ONLY template <typename tPayload = tStateInput, typename std::enable_if_t<std::is_void<tPayload>::value>* = nullptr>
#define PREDICATE_ONLY template <typename tPredicateFn, typename std::enable_if_t<!std::is_convertible<tStateInput, tPredicateFn>::value>* = nullptr>
/// @endcond

	// Link methods
	VOID_ONLY LinkHandle Link() //< Empty predicate link (always follow link)
	{
		return _InternalLink([] { return true; }, LinkHandle::eType::Normal);
	}
	NONVOID_ONLY LinkHandle Link(tPayload in_payload) //< Empty predicate link w/ payload (always follow link, using provided payload)
	{
		return _InternalLink([payload = MoveTemp(in_payload)]() -> tPredicateRet { return payload; }, LinkHandle::eType::Normal);
	}
	PREDICATE_ONLY LinkHandle Link(tPredicateFn in_predicate) //< Predicate link w/ implicit payload (follow link when predicate returns a value; use return value as payload)
	{
		return _InternalLink(in_predicate, LinkHandle::eType::Normal);
	}
	NONVOID_ONLY_WITH_PREDICATE LinkHandle Link(tPredicateFn in_predicate, tPayload in_payload) //< Predicate link w/ explicit payload (follow link when predicate returns true; use provided payload)
	{
		return _InternalLink(in_predicate, MoveTemp(in_payload), LinkHandle::eType::Normal);
	}

	// OnCompleteLink methods
	VOID_ONLY LinkHandle OnCompleteLink() //< Empty predicate link (always follow link)
	{
		return _InternalLink([] { return true; }, LinkHandle::eType::OnComplete);
	}
	NONVOID_ONLY LinkHandle OnCompleteLink(tPayload in_payload) //< Empty predicate link w/ payload (always follow link, using provided payload)
	{
		return _InternalLink([payload = MoveTemp(in_payload)]() -> tPredicateRet { return payload; }, LinkHandle::eType::OnComplete);
	}
	PREDICATE_ONLY LinkHandle OnCompleteLink(tPredicateFn in_predicate) //< Predicate link w/ implicit payload (follow link when predicate returns a value; use return value as payload)
	{
		return _InternalLink(in_predicate, LinkHandle::eType::OnComplete, true);
	}
	NONVOID_ONLY_WITH_PREDICATE LinkHandle OnCompleteLink(tPredicateFn in_predicate, tPayload in_payload) //< Predicate link w/ explicit payload (follow link when predicate returns true; use provided payload)
	{
		return _InternalLink(in_predicate, MoveTemp(in_payload), LinkHandle::eType::OnComplete, true);
	}

private:
	friend class ::TaskFSM;

	StateHandle() = delete;
	StateHandle(TSharedPtr<State<tStateInput, tStateConstructorFn>> InStatePtr)
		: m_state(InStatePtr)
	{
	}
	StateHandle(const StateHandle& Other) = delete;
	StateHandle& operator=(const StateHandle& Other) = delete;

	// Internal link function implementations
	VOID_ONLY_WITH_PREDICATE LinkHandle _InternalLink(tPredicateFn in_predicate, LinkHandle::eType in_linkType, bool in_isConditional = false) // bool-returning predicate
	{
		static_assert(std::is_same<bool, decltype(in_predicate())>::value, "This link requires a predicate function returning bool");
		TSharedPtr<LinkBase> link = MakeShared<FSM::Link<tStateInput, tStateConstructorFn, tPredicateFn>>(m_state, in_predicate);
		return LinkHandle(link, in_linkType, in_isConditional);
	}
	NONVOID_ONLY_WITH_PREDICATE LinkHandle _InternalLink(tPredicateFn in_predicate, LinkHandle::eType in_linkType, bool in_isConditional = false) // optional-returning predicate
	{
		static_assert(std::is_same<TOptional<tStateInput>, decltype(in_predicate())>::value, "This link requires a predicate function returning TOptional<tStateInput>");
		TSharedPtr<LinkBase> link = MakeShared<FSM::Link<tStateInput, tStateConstructorFn, tPredicateFn>>(m_state, in_predicate);
		return LinkHandle(link, in_linkType, in_isConditional);
	}
	NONVOID_ONLY_WITH_PREDICATE LinkHandle _InternalLink(tPredicateFn in_predicate, tPayload in_payload, LinkHandle::eType in_linkType, bool in_isConditional = false) // bool-returning predicate w/ fixed payload
	{
		static_assert(std::is_same<bool, decltype(in_predicate())>::value, "This link requires a predicate function returning bool");
		auto predicate = [in_predicate, in_payload]() -> TOptional<tStateInput>
		{
			return in_predicate() ? TOptional<tStateInput>(in_payload) : TOptional<tStateInput>{};
		};
		return _InternalLink(predicate, in_linkType, in_isConditional);
	}

	// SFINAE Template Declaration Macros (#undefs)
#undef NONVOID_ONLY_WITH_PREDICATE
#undef VOID_ONLY_WITH_PREDICATE
#undef NONVOID_ONLY
#undef VOID_ONLY
#undef PREDICATE_ONLY

	TSharedPtr<State<tStateInput, tStateConstructorFn>> m_state; // Internal state object
};

} // namespace FSM

using StateId = FSM::StateId;
template<class tStateInput, class tStateConstructorFn>
using StateHandle = FSM::StateHandle<tStateInput, tStateConstructorFn>;
using TransitionDebugData = FSM::TransitionDebugData;
using tOnStateTransitionFn = FSM::tOnStateTransitionFn;

//--- TaskFSM ---//
class TaskFSM
{
public:
	using tOnStateTransitionFn = TFunction<void()>;
	using tDebugStateTransitionFn = TFunction<void(TransitionDebugData)>;

	// Create a new FSM state [fancy param-deducing version (hopefully) coming soon!]
	template<typename tStateConstructorFn>
	auto State(FString in_name, tStateConstructorFn in_stateCtorFn)
	{
		typedef FSM::function_traits<tStateConstructorFn> tFnTraits;
		using tStateInput = typename tFnTraits::tArg;
		const FSM::StateId newStateId = m_states.Num();
		m_states.Add(InternalStateData(in_name));
		auto state = MakeShared<FSM::State<tStateInput, tStateConstructorFn>>(MoveTemp(in_stateCtorFn), newStateId, in_name);
		return FSM::StateHandle<tStateInput, tStateConstructorFn>{ state };
	}

	// Create a new FSM exit state (immediately terminates the FSM when executed)
	FSM::StateHandle<void, void> State(FString in_name)
	{
		const FSM::StateId newStateId = m_states.Num();
		m_states.Add(InternalStateData(in_name));
		m_exitStates.Add(newStateId);
		auto state = MakeShared<FSM::State<void, void>>(newStateId, in_name);
		return FSM::StateHandle<void, void>{ state };
	}

	// Define the initial entry links into the state machine
	void EntryLinks(TArray<FSM::LinkHandle> in_entryLinks);

	// Define all outgoing links from a given state (may only be called once per state)
	template<class tStateInput, class tStateConstructorFn>
	void StateLinks(const FSM::StateHandle<tStateInput, tStateConstructorFn>& in_originState, TArray<FSM::LinkHandle> in_outgoingLinks);

	// Begins execution of the state machine (returns id of final exit state)
	Task<FSM::StateId> Run(tOnStateTransitionFn in_onTransitionFn = {}, tDebugStateTransitionFn in_debugStateTransitionFn = {}) const;

private:
	// Evaluates all possible outgoing links from the current state, returning the first valid transition (if any transitions are valid)
	TOptional<FSM::TransitionEvent> EvaluateLinks(FSM::StateId in_curStateId, bool in_isCurrentStateComplete, const tOnStateTransitionFn& in_onTransitionFn) const;

	// Internal state
	struct InternalStateData
	{
		InternalStateData(FString in_debugName)
			: debugName(in_debugName)
		{
		}
		TArray<FSM::LinkHandle> outgoingLinks;
		FString debugName;
	};
	TArray<InternalStateData> m_states;
	TArray<FSM::LinkHandle> m_entryLinks;
	TArray<FSM::StateId> m_exitStates;
};

/// @} end of group TaskFSM

//--- TaskFSM Methods ---//
template<class tStateInput, class tStateConstructorFn>
void TaskFSM::StateLinks(const FSM::StateHandle<tStateInput, tStateConstructorFn>& in_originState, TArray<FSM::LinkHandle> in_outgoingLinks)
{
	const int32_t stateIdx = in_originState.m_state->stateId.idx;
	SQUID_RUNTIME_CHECK(m_states[stateIdx].outgoingLinks.Num() == 0, "Cannot set outgoing links more than once for each state");

	// Validate that there are exactly 0 or 1 unconditional OnComplete links (there may be any number of other OnComplete links, but only one with no condition)
	int32_t numOnCompleteLinks = 0;
	int32_t numOnCompleteLinks_Unconditional = 0;
	for(const FSM::LinkHandle& link : in_outgoingLinks)
	{
		if(link.IsOnCompleteLink())
		{
			SQUID_RUNTIME_CHECK(numOnCompleteLinks_Unconditional == 0, "Cannot call OnCompleteLink() after calling OnCompleteLink() with no conditions (unreachable link)");
			++numOnCompleteLinks;
			if(!link.HasCondition())
			{
				numOnCompleteLinks_Unconditional++;
			}
		}
	}
	SQUID_RUNTIME_CHECK(numOnCompleteLinks == 0 || numOnCompleteLinks_Unconditional > 0, "More than one unconditional OnCompleteLink() was set");

	// Set the outgoing links for the origin state
	m_states[stateIdx].outgoingLinks = MoveTemp(in_outgoingLinks);
}
inline void TaskFSM::EntryLinks(TArray<FSM::LinkHandle> in_entryLinks)
{
	// Validate to ensure there are no OnComplete links set as entry links
	int32_t numOnCompleteLinks = 0;
	for(const FSM::LinkHandle& link : in_entryLinks)
	{
		if(link.IsOnCompleteLink())
		{
			++numOnCompleteLinks;
		}
	}
	SQUID_RUNTIME_CHECK(numOnCompleteLinks == 0, "EntryLinks() list may not contain any OnCompleteLink() links");

	// Set the entry links list for this FSM
	m_entryLinks = MoveTemp(in_entryLinks);
}
inline TOptional<FSM::TransitionEvent> TaskFSM::EvaluateLinks(FSM::StateId in_curStateId, bool in_isCurrentStateComplete, const tOnStateTransitionFn& in_onTransitionFn) const
{
	// Determine whether to use entry links or state-specific outgoing links
	const TArray<FSM::LinkHandle>& links = (in_curStateId.idx < m_states.Num()) ? m_states[in_curStateId.idx].outgoingLinks : m_entryLinks;

	// Find the first valid transition from the current state
	for(const FSM::LinkHandle& link : links)
	{
		if(!link.IsOnCompleteLink() || in_isCurrentStateComplete) // Skip link evaluation check for OnComplete links unless current state is complete
		{
			if(auto result = link.EvaluateLink(in_onTransitionFn)) // Check if the transition to this state is valid
			{
				return result;
			}
		}
	}
	return {}; // No valid transition was found
}
inline Task<FSM::StateId> TaskFSM::Run(tOnStateTransitionFn in_onTransitionFn, tDebugStateTransitionFn in_debugStateTransitionFn) const
{
	// Task-local variables
	FSM::StateId curStateId; // The current state's ID
	Task<> task; // The current state's task

	// Custom debug task name logic
	TASK_NAME(__FUNCTION__, [this, &curStateId, &task]
	{
		const auto stateName = m_states.IsValidIndex(curStateId.idx) ? m_states[curStateId.idx].debugName : "";
		return FString::Printf(TEXT("%s -- %s"), *stateName, *task.GetDebugStack());
	});

	// Debug state transition lambda
	auto DebugStateTransition = [this, in_debugStateTransitionFn](FSM::StateId in_oldStateId, FSM::StateId in_newStateId) {
		if(in_debugStateTransitionFn)
		{
			FString oldStateName = in_oldStateId.IsValid() ? m_states[in_oldStateId.idx].debugName : FString("<ENTRY>");
			FString newStateName = m_states[in_newStateId.idx].debugName;
			in_debugStateTransitionFn({ in_oldStateId, MoveTemp(oldStateName), in_newStateId, MoveTemp(newStateName) });
		}
	};

	// Main FSM loop
	while(true)
	{
		// Evaluate links, checking for a valid transition
		if(TOptional<FSM::TransitionEvent> transition = EvaluateLinks(curStateId, task.IsDone(), in_onTransitionFn))
		{
			auto newStateId = transition->newStateId;
			DebugStateTransition(curStateId, newStateId); // Call state-transition debug function

			// If the transition is to an exit state, return that state ID (terminating the FSM)
			if(m_exitStates.Contains(newStateId.idx))
			{
				co_return newStateId;
			}
			SQUID_RUNTIME_CHECK(newStateId.idx < m_states.Num(), "It should be logically impossible to get an invalid state to this point");

			// Begin running new state (implicitly killing old state)
			curStateId = newStateId;
			co_await RemoveStopTask(task);
			task = MoveTemp(transition->newTask); // NOTE: Initial call to Resume() happens below
			co_await AddStopTask(task);
		}

		// Resume current state
		task.Resume();

		// Suspend until next frame
		co_await Suspend();
	}
}

NAMESPACE_SQUID_END
