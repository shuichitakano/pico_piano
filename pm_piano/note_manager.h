/*
 * author : Shuichi TAKANO
 * since  : Sat May 11 2019 22:34:15
 */
#ifndef _103DE5E1_1134_152A_154E_889BBA4369C5
#define _103DE5E1_1134_152A_154E_889BBA4369C5

#include "note.h"
#include "pedal.h"
#include "sys_params.h"
#include <array>
#include <vector>

#include "pico/sync.h"

namespace physical_modeling_piano
{
    class NoteManager
    {
        static constexpr int NOTE_BEGIN = 21;
        static constexpr int NOTE_END = 109;
        static constexpr size_t N_NOTES = NOTE_END - NOTE_BEGIN;

        std::array<Note, N_NOTES> notes_;
        std::array<int8_t, N_NOTES> noteNode_;
        std::array<bool, N_NOTES> keyOnStateForDisp_;

        struct Node
        {
            Note::State state_;
            int noteIndex_{};

            Node *prev_{};
            Node *next_{};
        };

        std::vector<Node> nodes_;
        Node *free_{};   // 片方向
        Node *active_{}; // 双方向
        Node *activeTail_{};

        const SystemParameters *currentSysParams_{};
        const PedalState *currentPedalState_{};
        std::vector<Node *> workNodes_;

        mutable int workIdx_;
        mutable bool workerActive_ = false;

        std::vector<Note::SampleT> workerSamples_{};

        size_t currentNoteCount_{};

        critical_section_t cs_;

    public:
        void initialize(const SystemParameters &sysParams, size_t nPoly);
        void __time_critical_func(keyOn)(int note, Hammer::VelocityT v);
        void __time_critical_func(keyOff)(int note);

        void __time_critical_func(update)(Note::SampleT *samples,
                                          size_t nSamples,
                                          const SystemParameters &sysParams,
                                          const PedalState &pedal);

        size_t getCurrentNoteCount() const { return currentNoteCount_; }
        const std::array<bool, N_NOTES> &getKeyOnStateForDisp() const
        {
            return keyOnStateForDisp_;
        }

        void __time_critical_func(worker)();

    protected:
        int __time_critical_func(getNodeIndex)(Node *node) const;

        Node *__time_critical_func(allocateNode)();
        void __time_critical_func(freeNode)(Node *node);
        void __time_critical_func(pushActive)(Node *node);
        void __time_critical_func(pushFrontActive)(Node *node);
        Node *__time_critical_func(popFrontActive)();
        void __time_critical_func(removeActive)(Node *node);

        int __time_critical_func(process)(Note::SampleT *samples, size_t nSamples);
    };

} // namespace physical_modeling_piano

#endif /* _103DE5E1_1134_152A_154E_889BBA4369C5 */
