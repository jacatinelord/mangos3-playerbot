#pragma once

class PlayerbotAI;  // Forward declaration

namespace ai
{
    /**
     * @brief A class that makes the AI aware of the PlayerbotAI instance.
     */
    class PlayerbotAIAware
    {
    public:
        /**
         * @brief Constructor for PlayerbotAIAware.
         * @param ai Pointer to the PlayerbotAI instance.
         */
        PlayerbotAIAware(PlayerbotAI* ai) : ai(ai) { }

        virtual ~PlayerbotAIAware() = default;

    protected:
        PlayerbotAI* ai; ///< Pointer to the PlayerbotAI instance.
    };
}
