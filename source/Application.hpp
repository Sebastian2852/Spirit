#pragma once

#include "CollisionSystem.hpp"
#include "Input.hpp"
#include "MeshSystem.hpp"
#include "PhysicsSystem.hpp"
#include "Renderer.hpp"
#include "Storage.hpp"
#include "TextureSystem.hpp"

#include "Logger.hpp"

#include <Chrono>

// Application keeps track of timing and running the simulation loop and runtime of the program.
class Application
{
public:
    typedef std::chrono::steady_clock Clock;
    Application();
    void simulationLoop();

private:
    System::TextureSystem mTextureSystem;
    System::MeshSystem mMeshSystem;
    ECS::Storage mStorage;

    System::CollisionSystem mCollisionSystem;
    System::PhysicsSystem mPhysicsSystem;

    Renderer mRenderer;
    Input mInput;

    bool mPhysicsTimeStepChanged                  = false; // True when the physics timestep is changed, causes an exit from the loop and re-run
    int mPhysicsTicksPerSecond                    = 60;    // The number of physics updates to perform per second. This is the template argument passed to simulationLoop pPhysicsTicksPerSecond.
    std::chrono::duration<double> mRenderTimestep = std::chrono::duration<double>(std::chrono::seconds(1)) / mRenderer.mTargetFPS;
    std::chrono::milliseconds maxFrameDelta       = std::chrono::milliseconds(250); // If the time between loops is beyond this, cap at this duration

    // This simulation loop uses a physics timestep based on integer type giving no truncation or round-off error.
    // It's required to be templated to allow physicsTimestep to be set using std::ratio as the chrono::duration period.
    // pPhysicsTicksPerSecond: The number of physics ticks per second. This template parameter is always equivalent to mPhysicsTicksPerSecond.
    template <int pPhysicsTicksPerSecond>
    void simulationLoop()
    {
        auto physicsTimestep = std::chrono::duration<Clock::rep, std::ratio<1, pPhysicsTicksPerSecond>>{1};

        LOG_INFO("Physics ticks per second: {}" , pPhysicsTicksPerSecond);
        LOG_INFO("Physics fixed timestep: {}ms" , std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(physicsTimestep).count());
        LOG_INFO("Renderer FPS: {}"             , mRenderer.mTargetFPS);
        LOG_INFO("Render timestep: {}ms"        , std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(mRenderTimestep).count());

        // The resultant sum of a Clock::duration and physicsTimestep. This will be the coarsest precision that can exactly represent both
        // a Clock::duration and 1/60 of a second. Time-based arithmetic will have no truncation error, or any round-off error
        // if Clock::duration is integral-based (std::chrono::nanoseconds for steady_clock is integral based).
        typedef decltype(Clock::duration{} + physicsTimestep)   Duration;
        typedef std::chrono::time_point<Clock, Duration>        TimePoint;

        Duration durationSinceLastPhysicsTick   = Duration::zero();     // Accumulated time since the last physics update.
        Duration durationSinceLastRenderTick    = Duration::zero();     // Accumulated time since the last render.
        Duration durationSinceLastFrame         = Duration::zero();     // Time between this frame and last frame.
        Duration durationApplicationRunning     = Duration::zero();     // Total time the application has been running.

        TimePoint physicsTime{};      // The time point the physics is advanced to currently.
        TimePoint timeFrameStarted{}; // The time point at the start of a new frame.
        TimePoint timeLastFrameStarted = Clock::now();

        // Continuous loop until the main window is marked for closing or Input requests close.
        // While looping the physics updates by fixed timestep physicsTimestep
        // The renderer produces time and the simulation consumes it in discrete physicsTimestep sized steps
        while (true)
        {
            mInput.pollEvents();
            if (mInput.closeRequested() || mPhysicsTimeStepChanged)
                break;

            timeFrameStarted = Clock::now();
            durationSinceLastFrame = timeFrameStarted - timeLastFrameStarted;
            if (durationSinceLastFrame > maxFrameDelta)
                durationSinceLastFrame = maxFrameDelta;

            timeLastFrameStarted            = timeFrameStarted;
            durationApplicationRunning      += durationSinceLastFrame;
            durationSinceLastPhysicsTick    += durationSinceLastFrame;
            durationSinceLastRenderTick     += durationSinceLastFrame;

            // Apply physics updates until accumulated time is below physicsTimestep step
            while (durationSinceLastPhysicsTick >= mRenderTimestep)
            {
                durationSinceLastPhysicsTick -= physicsTimestep;
                physicsTime                  += physicsTimestep;

                //previousState = currentState;
                //integrate(currentState, physicsTime, physicsTimestep);
                mPhysicsSystem.integrate(std::chrono::duration_cast<System::PhysicsSystem::DeltaTime>(physicsTimestep));
                mCollisionSystem.checkCollisions();
            }

            if (durationSinceLastRenderTick >= mRenderTimestep)
            {
                // Any remainder in the durationSinceLastPhysicsTick is a measure of how much more time is required before another physics step can be taken
                // Next interpolate between the previous and current physics state based on how much time is left in the durationSinceLastPhysicsTick
                // preventing a subtle but visually unpleasant stuttering of the physics simulation on the screen
                const double alpha = std::chrono::duration<double>{durationSinceLastPhysicsTick} / physicsTimestep; // Blending factor between 0-1 used to interpolate current state
                //renderState = currentState * alpha + previousState * (1 - alpha);
                //mRenderer.draw(renderState);

                mRenderer.draw(std::chrono::round<std::chrono::microseconds>(durationSinceLastRenderTick));
                durationSinceLastRenderTick = Duration::zero();
            }
        }

        const double totalTimeSeconds = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1,1>> >(durationApplicationRunning).count();
        const double renderFPS = static_cast<double>(mRenderer.mDrawCount) / totalTimeSeconds;
        const double physicsFPS = static_cast<double>(mPhysicsSystem.mUpdateCount) / totalTimeSeconds;

        LOG_INFO("------------------------------------------------------------------------");
        LOG_INFO("Total simulation time: {}s", totalTimeSeconds);
        LOG_INFO("Total physics updates: {}", mPhysicsSystem.mUpdateCount);
        LOG_INFO("Averaged physics updates per second: {}/s", physicsFPS);
        LOG_INFO("Total rendered frames: {}", mRenderer.mDrawCount);
        LOG_INFO("Averaged render frames per second: {}/s", renderFPS);
    }
};