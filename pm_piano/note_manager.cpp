/*
 * author : Shuichi TAKANO
 * since  : Sat May 11 2019 22:39:33
 */

#include "note_manager.h"
#include <algorithm>
#include <assert.h>

#include "hardware/gpio.h"

namespace physical_modeling_piano
{

    namespace
    {
        enum Event
        {
            START = 1 << 0,
            SYNC = 1 << 1,
        };
    }

    void
    NoteManager::initialize(const SystemParameters &sysParams, size_t nPoly)
    {
        size_t allocatorSize = 0;

        for (int i = 0; i < N_NOTES; ++i)
        {
            float f = 440 * powf(2.0f, (i + NOTE_BEGIN - 69) / 12.0f);
            notes_[i].initialize(f, sysParams);

            allocatorSize =
                std::max(allocatorSize, notes_[i].computeAllocatorSize());
        }

        printf("note %zd bytes, notes %zd, st %zd, allocator %zd\n",
               sizeof(Note),
               sizeof(notes_),
               sizeof(Note::State),
               allocatorSize);

        std::fill(noteNode_.begin(), noteNode_.end(), -1);

        nodes_.resize(nPoly);
        for (auto &&n : nodes_)
        {
            n.state_.initialize(allocatorSize);
            freeNode(&n);
        }

        workNodes_.resize(nPoly);

        critical_section_init(&cs_);
    }

    void
    NoteManager::update(Note::SampleT *samples,
                        size_t nSamples,
                        const SystemParameters &sysParams,
                        const PedalState &pedal)
    {
#if 0
        auto *node = active_;
        while (node)
        {
            //        printf("update %p, %d\n", node, getNodeIndex(node));
            auto noteIdx = node->noteIndex_;
            notes_[noteIdx].update(
                samples, nSamples, node->state_, sysParams, pedal);

            if (node->state_.idle)
            {
                //            printf("to idle %d\n", noteIdx + 21);
                noteNode_[noteIdx] = -1;

                auto next = node->next_;
                removeActive(node);
                freeNode(node);
                node = next;
            }
            else
            {
                node = node->next_;
            }
        }
#else
        // 前回スキップしたのがまだ終わってないことがある
        while (workerActive_)
        {
            __wfe();
        }

        workNodes_.clear();
        auto *node = active_;
        while (node)
        {
            workNodes_.push_back(node);
            node = node->next_;
        }

        workerSamples_.resize(nSamples);
        std::fill(workerSamples_.begin(), workerSamples_.end(), 0);

        currentSysParams_ = &sysParams;
        currentPedalState_ = &pedal;

        workIdx_ = 0;
        if (!workNodes_.empty())
        {
            workerActive_ = true;
            __sev();
        }

        int nn = process(samples, nSamples);
        //    printf("mn = %d\n", nn);
        (void)nn;

        if (nn < workNodes_.size())
        {
            while (workerActive_)
            {
                __wfe();
                //            tight_loop_contents();
            }
        }

        int n = 0;
        node = active_;
        while (node)
        {
            if (node->state_.idle)
            {
                noteNode_[node->noteIndex_] = -1;
                keyOnStateForDisp_[node->noteIndex_] = false;

                auto next = node->next_;
                removeActive(node);
                freeNode(node);
                node = next;
            }
            else
            {
                node = node->next_;
                ++n;
            }
        }
        currentNoteCount_ = n;

        const auto *ws = workerSamples_.data();
        do
        {
            add(*samples, *samples, *ws);
            ++samples;
            ++ws;
        } while (--nSamples);

#endif
    }

#if 1
    int
    NoteManager::process(Note::SampleT *samples, size_t nSamples)
    {
        int ct = 0;
        auto n = workNodes_.size();
        while (1)
        {
            critical_section_enter_blocking(&cs_);
            auto idx = workIdx_;
            ++workIdx_;
            critical_section_exit(&cs_);

            if (idx >= n)
            {
                return ct;
            }

            auto *node = workNodes_[idx];
            auto noteIdx = node->noteIndex_;
            notes_[noteIdx].update(samples,
                                   nSamples,
                                   node->state_,
                                   *currentSysParams_,
                                   *currentPedalState_);
            ++ct;
        }
    }

    void
    NoteManager::worker()
    {
        while (1)
        {
            while (!workerActive_)
            {
                __wfe();
                //                tight_loop_contents();
            }
            //            gpio_put(6, 1);
            auto irq = save_and_disable_interrupts();

            int nn = process(workerSamples_.data(), workerSamples_.size());
            //        printf("wn %d\n", nn);
            (void)nn;

            workerActive_ = false;
            __sev();
            //            gpio_put(6, 0);
            restore_interrupts(irq);
        }
    }
#endif

    void
    NoteManager::keyOn(int note, Hammer::VelocityT v)
    {
        note -= NOTE_BEGIN;
        if (note < 0 || note >= N_NOTES)
        {
            return;
        }

        Node *node;
        int nodeIndex = noteNode_[note];
        if (nodeIndex >= 0)
        {
            node = &nodes_[nodeIndex];
            assert(node->noteIndex_ == note);
        }
        else
        {
            node = allocateNode();
            if (!node)
            {
                node = popFrontActive();

                noteNode_[node->noteIndex_] = -1;
                keyOnStateForDisp_[node->noteIndex_] = false;
            }
            assert(node);

            node->noteIndex_ = note;
            noteNode_[note] = getNodeIndex(node);
            pushActive(node);
        }

        notes_[note].keyOn(node->state_, v);
        keyOnStateForDisp_[note] = true;

        // printf("allocated node: %p, idx %d, note %d\n",
        //        node,
        //        noteNode_[note],
        //        note + 21);
    }

    void
    NoteManager::keyOff(int note)
    {
        note -= NOTE_BEGIN;
        if (note < 0 || note >= N_NOTES)
        {
            return;
        }

        int nodeIndex = noteNode_[note];
        if (nodeIndex < 0)
        {
            return;
        }
        auto *node = &nodes_[nodeIndex];
        assert(node->noteIndex_ == note);

        notes_[note].keyOff(node->state_);

        // 先頭に持っていく
        if (active_ != node)
        {
            removeActive(node);
            pushFrontActive(node);
        }
        keyOnStateForDisp_[note] = false;
    }

    int
    NoteManager::getNodeIndex(Node *node) const
    {
        return node - nodes_.data();
    }

    NoteManager::Node *
    NoteManager::allocateNode()
    {
        if (free_)
        {
            auto r = free_;
            free_ = free_->next_;
            return r;
        }
        return nullptr;
    }

    void
    NoteManager::freeNode(Node *node)
    {
        node->next_ = free_;
        free_ = node;
    }

    void
    NoteManager::pushActive(Node *node)
    {
        if (activeTail_)
        {
            assert(active_);
            activeTail_->next_ = node;
            node->prev_ = activeTail_;
            node->next_ = nullptr;
            activeTail_ = node;
        }
        else
        {
            active_ = node;
            activeTail_ = node;
            node->prev_ = nullptr;
            node->next_ = nullptr;
        }
    }

    void
    NoteManager::pushFrontActive(Node *node)
    {
        if (active_)
        {
            assert(activeTail_);
            active_->prev_ = node;
            node->next_ = active_;
            node->prev_ = nullptr;
            active_ = node;
        }
        else
        {
            active_ = node;
            activeTail_ = node;
            node->prev_ = nullptr;
            node->next_ = nullptr;
        }
    }

    NoteManager::Node *
    NoteManager::popFrontActive()
    {
        if (!active_)
        {
            return nullptr;
        }
        auto r = active_;
        active_ = r->next_;
        if (active_)
        {
            active_->prev_ = nullptr;
        }
        else
        {
            assert(activeTail_ == r);
            activeTail_ = nullptr;
        }
        return r;
    }

    void
    NoteManager::removeActive(Node *node)
    {
        if (node->prev_)
        {
            node->prev_->next_ = node->next_;
        }
        else
        {
            assert(active_ == node);
            active_ = node->next_;
        }

        if (node->next_)
        {
            node->next_->prev_ = node->prev_;
        }
        else
        {
            assert(activeTail_ == node);
            activeTail_ = node->prev_;
        }
    }

} // namespace physical_modeling_piano
