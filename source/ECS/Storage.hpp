#pragma once

#include "Logger.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <iostream>
#include <optional>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include "Meta.hpp"

namespace ECS
{
    using EntityID = size_t;
    class Entity
    {
    public:
        EntityID ID;

        operator EntityID () const { return ID; } // Implicitly convert an Entity to an EntityID.
    };

    typedef size_t ArchetypeID;
    typedef size_t ArchetypeInstanceID; // Per ArchetypeID ID per component archetype instance.
    typedef size_t ComponentID;         // Unique identifier for any type passed into ECSStorage.
    typedef std::bitset<32> ComponentBitset;
    typedef size_t BufferPosition; // Used to index into archetype mBuffer.
    typedef unsigned char Byte;

    class ComponentIDGenerator
    {
        static inline ComponentID counter = 0;

        template<typename ComponentType>
        static inline ComponentID perComponentTypeID = counter++;

    public:
        template<typename ComponentType>
        static ComponentID get()
        {
            typedef typename std::decay_t<ComponentType> DecayedComponentType;

            //LOG("ECS: Component '{}' ECS::ComponentID: {}",  std::type_index(typeid(DecayedComponentType)).name(), perComponentTypeID<DecayedComponentType>);
            return perComponentTypeID<DecayedComponentType>;
        }
    };

    // Generates a bitset out of all the ComponentTypes. Skips over Entity params.
    template <typename... ComponentTypes>
    static ComponentBitset getBitset()
    {
        ComponentBitset componentBitset;

        auto setComponentBit = [&componentBitset]<typename ComponentType>()
        {
            if constexpr (!std::is_same_v<Entity, std::decay_t<ComponentType>>) // Ignore any Entity params supplied.
                componentBitset.set(ComponentIDGenerator::get<ComponentType>());
        };
        (setComponentBit.operator() < ComponentTypes > (), ...);

        return componentBitset;
    }

    // A container of Entity objects and the components they own.
    // Every unique combination of components makes an Archetype which is a contiguouse store of all the ComponentTypes.
    // Storage is interfaced using Entity as a key.
    class Storage
    {
        // Archetype is defined as a unique combination of ComponentTypes. It is a non-templated class allowing any combination of unique types to be stored in its mBuffer at runtime.
        // The ComponentTypes are retrievable using getComponent and getComponentImpl as well as their 'Mutable' variants.
        // Every archetype stores its mBitset for matching ComponentTypes.
        // mComponentLayout sets out how those ComponentTypes are laid out in mBuffer.
        struct Archetype
        {
            // Destructor wraps a pointer to a destructor for DesctructorType with type erasure. Calling Destroy with an address of a DesctructorType instance will call its destructor.
            // There is no safety for calling Destroy with a non-DestructorType instance address.
            // This is a modified version of https://herbsutter.com/2016/09/25/to-store-a-destructor/
            struct Destructor
            {
                Destructor() : Destroy([](const void*) {}) {}

                template <typename DestructorType>
                Destructor(Meta::PackArg<DestructorType>)
                    : Destroy{ [](const void* pInstanceAddress)
                        {
                            using Type = std::decay_t<DestructorType>;
                            // There is no possible runtime check to find if pInstanceAddress points to a type of DestructorType.
                            // In the above Herb Sutter code, the Destructor class also stores the address to use and avoid this lack of 'type-safety'.
                            static_cast<const Type*>(pInstanceAddress)->~Type();
                        } }
                {}

                void (*Destroy)(const void*); // Pointer to the destructor function
            };

            // How a given ComponentType in any ArchetypeInstanceID is stored in mBuffer.
            // ComponentLayout are constructed in processComponentsIntoArchetypeLayout.
            struct ComponentLayout
            {
                ComponentLayout() : mComponentID{ 0 }, mOffset{ 0 }, mDestructor{}
                {}

                template <typename DestructorType>
                ComponentLayout(const BufferPosition& pOffset, Meta::PackArg<DestructorType>)
                    : mComponentID{ ComponentIDGenerator::get<DestructorType>() }
                    , mOffset(pOffset)
                    , mDestructor{ Meta::PackArg<DestructorType>() }
                {}

                ComponentID mComponentID; // Unique ID of the type.
                BufferPosition mOffset;   // Byte offset from the start of the ArchetypeInstanceID to this componentType.
                Destructor mDestructor;   // Function pointer to the destructor for the type.
            };

            // Take the list of ComponentTypes and returns an array of their ComponentLayouts.
            // Packs the ComponentTypes tightly with 0 gaps in the same order they come in as in the parameter pack.
            template <typename... ComponentTypes>
            static std::array<ComponentLayout, sizeof...(ComponentTypes)> processComponentsIntoArchetypeLayout()
            {
                // #Optimisation: sort pComponentOrder in order of size for best padding?
                // #Optimisation: Index the components by word size offsets.

                size_t i = 0;
                size_t byteOffset = 0;
                std::array<ComponentLayout, sizeof...(ComponentTypes)> componentLayouts;

                (void(componentLayouts[i++] =
                    ComponentLayout(
                        (byteOffset += sizeof(ComponentTypes)) - sizeof(ComponentTypes)
                        , Meta::PackArg<ComponentTypes>()))
                    , ...);

                return componentLayouts;
            }

            const ComponentBitset mBitset;                       // The unique identifier for this archetype. Each bit corresponds to a ComponentType this archetype stores per ArchetypeInstanceID.
            const std::vector<ComponentLayout> mComponentLayout; // How the components are laid out in each ArchetypeInstanceID. The .size() of this vector tells us the number of unique ComponentTypes in an ArchetypeInstanceID.
            const size_t mInstanceSize;                          // Size in Bytes of each archetype instance. In other words, the stride between every ArchetypeInstanceID.
            ArchetypeInstanceID mNextInstanceID;                 // The ArchetypeInstanceID past the end of the mBuffer.
            std::vector<Byte> mBuffer;
            std::vector<Entity> mEntities;                      // Entity at every ArchetypeInstanceID. Should be indexed only using ArchetypeInstanceID.

            // Construct an Archetype from a list of ComponentTypes. The order the ComponentTypes will be packed in mBuffer is determined in processComponentsIntoArchetypeLayout() function.
            template<typename... ComponentTypes>
            Archetype(Meta::PackArgs<ComponentTypes...>)
                : mBitset(getBitset<ComponentTypes...>())
                , mComponentLayout(Meta::makeVector(processComponentsIntoArchetypeLayout<ComponentTypes...>()))
                , mInstanceSize(Meta::sizeOfVariadic<ComponentTypes...>())
                , mNextInstanceID(0)
                , mBuffer{}
                , mEntities{}
            {
                mBuffer.resize(mInstanceSize); // Reserve enough space to push 1 instance.

                std::string components      = "";
                std::string componentLayout = "|";
                for (size_t i = 0; i < mComponentLayout.size(); i++)
                {
                    components.empty() ? components = std::to_string(mComponentLayout[i].mComponentID) : components += ", " + std::to_string(mComponentLayout[i].mComponentID);

                    size_t size = 0;
                    if (mComponentLayout.size() == 1)
                        size = mInstanceSize;
                    else if (i == 0)
                        size = mComponentLayout[1].mOffset;
                    else if (i == mComponentLayout.size() - 1)
                        size = mInstanceSize - mComponentLayout[i].mOffset;
                    else
                        size = mComponentLayout[i + 1].mOffset - mComponentLayout[i].mOffset;

                    for (size_t j = 0; j < size; j++)
                    {
                        componentLayout += std::to_string(mComponentLayout[i].mComponentID);
                    }
                    componentLayout += '|';
                }
                //LOG("ECS: New Archetype created out of component combination ({}). Memory layout: {}", components, componentLayout);
            }

            // Search the mComponentLayout vector for the ComponentType and return its ComponentLayout.
            template <typename ComponentType>
            const ComponentLayout& getComponentLayout() const
            {
                const auto componentID = ComponentIDGenerator::get<ComponentType>();
                auto it = std::find_if(mComponentLayout.begin(), mComponentLayout.end(), [&componentID](const auto& pComponentLayout)
                    { return pComponentLayout.mComponentID == componentID; });

                if (it != mComponentLayout.end())
                    return *it;
                else
                {
                    throw std::logic_error("Requested a ComponentLayout for a ComponentType not present in this archetype.");
                    return mComponentLayout.front();
                }
            };
            // Get the byte offset of ComponentType from the start of any ArchetypeInstanceID.
            template <typename ComponentType>
            BufferPosition getComponentOffset() const
            {
                const auto& layout = getComponentLayout<ComponentType>();
                return layout.mOffset;
            }
            // Get the byte position of ComponentType at ArchetypeInstanceID.
            template <typename ComponentType>
            BufferPosition getComponentPosition(const ArchetypeInstanceID& pInstanceIndex) const
            {
                const auto indexStartPosition = mInstanceSize * pInstanceIndex; // Position of the start of the instance at pInstanceIndex.
                return indexStartPosition + getComponentOffset<ComponentType>();
            }

            // Returns a const pointer to a component of ComponentType at pInstanceIndex in the mBuffer.
            // The position of this component is found using a linear search of mComponentLayouts. If the BufferPosition is known use getComponentImpl directly.
            template <typename ComponentType>
            const std::decay_t<ComponentType>* getComponent(const ArchetypeInstanceID& pInstanceIndex) const
            {
                const auto componentStartPosition = getComponentPosition<ComponentType>(pInstanceIndex);
                return getComponentImpl<ComponentType>(componentStartPosition);
            }
            // Returns a const pointer to a component of ComponentType at pPosition in the mBuffer.
            // There is no guaratee pPosition points to the start of a type ComponentType. Only use this function when you are certain pPosition is the start of a ComponentType!
            // A valid pPosition can be retrieved using getComponentPosition.
            template <typename ComponentType>
            const std::decay_t<ComponentType>* getComponentImpl(const BufferPosition& pPosition) const
            {
                return reinterpret_cast<const std::decay_t<ComponentType>*>(&mBuffer[pPosition]);
            }

            // Returns a pointer to a component of ComponentType at pPosition in the mBuffer.
            // There is no guaratee pPosition points to the start of a type ComponentType. Only use this function when you are certain pPosition is the start of a ComponentType!
            // A valid pPosition can be retrieved using getComponentPosition.
            template <typename ComponentType>
            std::decay_t<ComponentType>* getComponentMutable(const ArchetypeInstanceID& pInstanceIndex)
            {
                const auto componentStartPosition = getComponentPosition<ComponentType>(pInstanceIndex);
                return getComponentMutableImpl<ComponentType>(componentStartPosition);
            }
            // Returns a pointer to a component of ComponentType at pPosition in the mBuffer.
            // There is no guaratee pPosition points to the start of a type ComponentType. Only use this function when you are certain pPosition is the start of a ComponentType!
            // A valid pPosition can be retrieved using getComponentPosition.
            template <typename ComponentType>
            std::decay_t<ComponentType>* getComponentMutableImpl(const BufferPosition& pPosition)
            {
                return reinterpret_cast<std::decay_t<ComponentType>*>(&mBuffer[pPosition]);
            }

            // Assign pNewValue to the ComponentType at ArchetypeInstanceID.
            template <typename ComponentType>
            void assign(const ComponentType& pNewValue, const ArchetypeInstanceID& pInstanceIndex)
            {
                const auto componentStartPosition = getComponentPosition<ComponentType>(pInstanceIndex);

                if (mBuffer.capacity() < componentStartPosition + sizeof(pNewValue))
                    throw std::logic_error("Index out of range! Trying to assign to a component past the end of the archetype buffer.");

                // Use placement new here since the memory is already allocated in push_back.
                using DecayType = std::decay_t<ComponentType>;
                DecayType* comp = new(&mBuffer[componentStartPosition]) DecayType(pNewValue);
            }

            // Assign pComponentValues to a new ArchetypeInstanceID. The new ArchetypeInstanceID == mNextInstanceID.
            // If the new ArchetypeInstanceID extends beyond the end of mBuffer capacity, the buffer is doubled.
            template <typename... ComponentTypes>
            void push_back(const Entity& pEntity, ComponentTypes&&... pComponentValues)
            {
                static_assert(Meta::is_unique<ComponentTypes...>, "Non unique component types! Archetype can only push back a set of unique ComponentTypes");
                const auto endInstanceStartPosition = mInstanceSize * mNextInstanceID;

                // double the size of the mBuffer until there is enough space for a new instance of this archetype.
                while (endInstanceStartPosition + mInstanceSize > mBuffer.capacity())
                    mBuffer.resize(mBuffer.capacity() * 2);

                auto assignComponent = [&](auto& pComponent) { assign(pComponent, mNextInstanceID); };
                (assignComponent(pComponentValues), ...);

                mEntities.push_back(pEntity);
                mNextInstanceID++; // Only do this once for all the components
            }

            // Remove the instance of the archetype at pInstanceIndex.
            // moves the end ArchetypeInstanceID Components to the pInstanceIndex ArchetypeInstanceID.
            void erase(const ArchetypeInstanceID& pInstanceIndex)
            {
                // Call the destructors for all the components at pInstanceIndex
                for (size_t componentIndex = 0; componentIndex < mComponentLayout.size(); componentIndex++)
                {
                    const auto componentBufferPos = (pInstanceIndex * mInstanceSize) + mComponentLayout[componentIndex].mOffset;
                    const auto compAddress = &mBuffer[componentBufferPos];
                    mComponentLayout[componentIndex].mDestructor.Destroy(compAddress);
                }

                if (pInstanceIndex != mNextInstanceID - 1) // If erasing off the end, skip the move
                {
                    const auto eraseIndexStartPosition = mInstanceSize * pInstanceIndex; // Position of the start of the instance at pInstanceIndex.
                    const auto lastIndexStartPosition  = mInstanceSize * (mNextInstanceID - 1); // Position of the start of the last instance.
                    const auto lastIndexEndPosition    = lastIndexStartPosition + mInstanceSize; // Position of the end of the last instance.
                    // The 'end' positions above are actually 1 index over the range, this is ok as std::move moves the range which contains all the elements
                    // between first and last, including the element pointed by first but not the element pointed by last.

                    // Move the end buffer data into the index requested for remove.
                    std::move(mBuffer.begin() + lastIndexStartPosition, mBuffer.begin() + lastIndexEndPosition, mBuffer.begin() + eraseIndexStartPosition);
                    // We don't pop mIntanceSize off the end of mBuffer as by decrementing mNextInstanceID the next push_back will overwrite the data anyway.
                    // Reducing mNextInstanceID will also prevent forEach iterating past the end of the valid mBuffer range.

                    mEntities[pInstanceIndex] = std::move(mEntities[mEntities.size() - 1]); // Move the end EntityID into the erased index, pop back below.
                }

                mNextInstanceID--;
                mEntities.pop_back();
            }
        }; // class Archetype

        EntityID mNextEntity = 0;
        std::vector<Archetype> mArchetypes;
        // Maps EntityID to a position pair [ index in mArchetypes, ArchetypeInstanceID in archetype ].
        // Nullopt here means the entity was deleted.
        std::vector<std::optional<std::pair<ArchetypeID, ArchetypeInstanceID>>> mEntityToArchetypeID;

        template <typename... FunctionArgs>
        struct FunctionHelper;
        template <typename... FunctionArgs>
        struct FunctionHelper<Meta::PackArgs<FunctionArgs...>>
        {
            static_assert(Meta::is_unique<FunctionArgs...>, "Cannot construct a FunctionHelper from a list of types with duplicates. Are you calling foreach with repeating parameters?");
            static_assert(sizeof...(FunctionArgs) > 0, "Cannot construct a FunctionHelper with 0 types, are you calling foreach with 0 params?");

            static ComponentBitset getBitset()
            {
                return ECS::getBitset<FunctionArgs...>();
            }
            // Does this function take only one parameter of type Entity.
            constexpr static bool isEntityFunction()
            {
                if constexpr (sizeof...(FunctionArgs) == 1 && std::is_same_v<Entity, std::decay_t<typename Meta::GetNth<0, FunctionArgs...>::Type>>)
                    return true;
                else
                    return false;
            }
        };

        template <typename... FunctionArgs>
        struct ApplyFunction;
        template <typename Func, typename... FunctionArgs>
        struct ApplyFunction<Func, Meta::PackArgs<FunctionArgs...>>
        {
            static void applyToArchetype(const Func& pFunction, Archetype& pArchetype)
            {
                const auto indexSequence = std::index_sequence_for<FunctionArgs...>{};
                const auto offsets = getOffsets(pArchetype, indexSequence);
                impl(pFunction, pArchetype, offsets, indexSequence);
            }

        private:
            // Given a pFunction and pArchetype, calls pFunction on every ArchetypeInstanceID supplying the ComponentTypes as arguments.
            // pArchetypeOffsets: The mapping of pFunction arguments to their offsets per ArchetypeInstanceID.
            // index_sequence:    Provides a mechanism to execute a fold expression to retrieve all the arguments from the Archetype.
            template <std::size_t... Is>
            static void impl(const Func& pFunction, Archetype& pArchetype, const std::array<BufferPosition, sizeof...(FunctionArgs)>& pArchetypeOffsets, const std::index_sequence<Is...>&)
            { // If we have reached this point we can guarantee pArchetype contains all the components in FunctionArgs.
                for (ArchetypeInstanceID i = 0; i < pArchetype.mNextInstanceID; i++)
                    pFunction(*getFromArchetype<FunctionArgs>(pArchetype, i, pArchetypeOffsets[Is])...);
            }

            // Get a ComponentType* from pArchetype at pIndex with pOffset.
            template <typename ComponentType>
            static std::decay_t<ComponentType>* getFromArchetype(Archetype& pArchetype, const ArchetypeInstanceID& pIndex, const size_t& pOffset)
            {
                if constexpr (std::is_same_v<Entity, std::decay_t<ComponentType>>)
                    return &pArchetype.mEntities[pIndex];
                else
                    return reinterpret_cast<std::decay_t<ComponentType>*>(&pArchetype.mBuffer[(pArchetype.mInstanceSize * pIndex) + pOffset]);
            }

            // Assign the Byte offset of the ComponentType in pArchetype into pOffsets at pIndex. Skips over Entity's encountered.
            template <typename ComponentType, std::size_t... Is>
            static void setOffset(std::array<BufferPosition, sizeof...(FunctionArgs)>& pOffsets, const size_t& pIndex, const Archetype& pArchetype)
            {
                if constexpr (!std::is_same_v<Entity, std::decay_t<ComponentType>>) // Ignore any Entity params supplied.
                    pOffsets[pIndex] = pArchetype.getComponentOffset<ComponentType>();
            }

            // Construct an array of corresponding to the offset of each FunctionArgs into the archetype.
            // Entity types encountered will not be set but the index in the returned array will exist.
            template <std::size_t... Is>
            static std::array<BufferPosition, sizeof...(FunctionArgs)> getOffsets(const Archetype& pArchetype, const std::index_sequence<Is...>&)
            {
                std::array<BufferPosition, sizeof...(FunctionArgs)> offsets;
                (setOffset<FunctionArgs>(offsets, Is, pArchetype), ...);
                return offsets;
            }
        };

        // Find the ArchetypeID with the exact matching componentBitset.
        // Every Archetype has a unique bitset so we can guarantee only one exists.
        // Returns nullopt if this archtype hasnt been added to mArchetypes yet.
        std::optional<ArchetypeID> getMatchingArchetype(const ComponentBitset& pComponentBitset)
        {
            for (ArchetypeID i = 0; i < mArchetypes.size(); i++)
            {
                if (pComponentBitset == mArchetypes[i].mBitset)
                    return i;
            }

            return std::nullopt;
        };
        // Find the ArchetypeIDs of any Archetypes with the exact matching componentBitset or containing it.
        // Returns an empty vec if there are none.
        std::vector<ArchetypeID> getMatchingOrContainedArchetypes(const ComponentBitset& pComponentBitset)
        {
            std::vector<ArchetypeID> returnVec;

            for (ArchetypeID i = 0; i < mArchetypes.size(); i++)
            {
                if (pComponentBitset == mArchetypes[i].mBitset || ((pComponentBitset & mArchetypes[i].mBitset) == pComponentBitset))
                    returnVec.push_back(i);
            }

            return returnVec;
        };

    public:
        // Creates an Entity out of the ComponentTypes.
        // The ComponentTypes must all be unique, only one of each ComponentType can be owned by an Entity.
        // The ComponentTypes can be retrieved individually using getComponent or as a combination using foreach.
        template <typename... ComponentTypes>
        Entity addEntity(ComponentTypes&&... pComponents)
        {
            static_assert(Meta::is_unique<ComponentTypes...>);

            const std::bitset<32> componentsBitset = getBitset<ComponentTypes...>();
            auto archetypeID = getMatchingArchetype(componentsBitset);

            if (!archetypeID)
            {// No matching archetype was found we add a new one for this ComponentBitset.
                mArchetypes.push_back(Archetype(Meta::PackArgs<ComponentTypes...>()));
                archetypeID = mArchetypes.size() - 1;
            }

            const auto newEntity = Entity(mNextEntity++);
            auto& archetype = mArchetypes[archetypeID.value()];
            archetype.push_back(newEntity, std::forward<ComponentTypes>(pComponents)...);
            mEntityToArchetypeID.push_back(std::make_optional(std::make_pair(archetypeID.value(), archetype.mNextInstanceID - 1)));

            return newEntity;
        }
        // Removes pEntity from storage.
        // The associated Entity is then on invalid for invoking other Storage funcrions on.
        void deleteEntity(const Entity& pEntity)
        {
            const auto [archetype, eraseIndex] = *mEntityToArchetypeID[pEntity.ID];
            mArchetypes[archetype].erase(eraseIndex);

            { // Update Storage bookeeping after the remove (mEntityToArchetypeID)
                if (eraseIndex != mArchetypes[archetype].mNextInstanceID) // If we removed from the end, no need to update mEntityToArchetypeID
                {
                    // After Archetype::erase, the EntityID at eraseIndex is the previously back ArchetypeInstance entity.
                    // Assign the movedEntityID the correct index in the archetype (the erased entity index)
                    auto& movedEntityID = mArchetypes[archetype].mEntities[eraseIndex].ID;
                    mEntityToArchetypeID[movedEntityID]->second = std::move(mEntityToArchetypeID[pEntity.ID]->second);
                }

                // mEntityToArchetypeID is index alligned to EntityID's we cannot erase the index corresponding to pEntity, instead set to nullopt.
                mEntityToArchetypeID[pEntity.ID] = std::nullopt;
            }
        }

        // Calls Func on every Entity which owns all of the components arguments of pFunction.
        // pFunction can have any number of ComponentTypes but will only be called if the Entity owns all of the components or more.
        // An optional Entity param in function will be supplied the Entity which owns the ComponentTypes on each call of pFunction.
        template <typename Func>
        void foreach(const Func& pFunction)
        {
            using FunctionParameterPack = typename Meta::GetFunctionInformation<Func>::GetParameterPack;

            const auto functionBitset = FunctionHelper<FunctionParameterPack>::getBitset();
            const auto archetypeIDs   = getMatchingOrContainedArchetypes(functionBitset);

            if (!archetypeIDs.empty())
            {
                for (auto& archetypeID : archetypeIDs)
                {
                    if (mArchetypes[archetypeID].mNextInstanceID > 0)
                    {
                        ApplyFunction<Func, FunctionParameterPack>::applyToArchetype(pFunction, mArchetypes[archetypeID]);
                    }
                }
            }
        };
        // Calls Func on every EntityID. Func must have only one parameter of type ECS::EntityID otherwise wont compile.
        template <typename Func>
        void foreachEntity(const Func& pFunction)
        {
            using FunctionParameterPack = typename Meta::GetFunctionInformation<Func>::GetParameterPack;
            static_assert(FunctionHelper<FunctionParameterPack>::isEntityFunction(), "pFunction is not a function that takes only one argument of type ECS::Entity");

            if (mNextEntity > 0)
            {
                for (EntityID i = 0; i < mNextEntity; i++)
                {
                    auto ent = Entity(i);
                    pFunction(ent);
                }
            }
        };

        // Get a const reference to component of ComponentType belonging to Entity.
        // If Entity doesn't own one exception will be thrown. Owned ComponentTypes can be queried using hasComponents.
        template <typename ComponentType>
        const std::decay_t<ComponentType>& getComponent(const Entity& pEntity) const
        {
            const auto [archetype, index] = *mEntityToArchetypeID[pEntity.ID];
            return *mArchetypes[archetype].getComponent<ComponentType>(index);
        }

        // Get a reference to component of ComponentType belonging to Entity.
        // If Entity doesn't own one exception will be thrown. Owned ComponentTypes can be queried using hasComponents.
        template <typename ComponentType>
        std::decay_t<ComponentType>& getComponentMutable(const Entity& pEntity)
        {
            const auto [archetype, index] = *mEntityToArchetypeID[pEntity.ID];
            return *mArchetypes[archetype].getComponentMutable<ComponentType>(index);
        }

        // Check if Entity has been assigned all of the ComponentTypes queried.
        template <typename... ComponentTypes>
        bool hasComponents(const Entity& pEntity) const
        {
            static_assert(sizeof...(ComponentTypes) != 0, "Cannot query hasComponents with 0 types.");

            if constexpr (sizeof...(ComponentTypes) > 1)
            {// Grab the archetype bitset the entity belongs to and check if the ComponentTypes bitset matches or is a subset of it.
                const auto requestedBitset = getBitset<ComponentTypes...>();
                const auto [archetype, index] = *mEntityToArchetypeID[pEntity.ID];
                const auto entityBitset = mArchetypes[archetype].mBitset;
                return (requestedBitset == entityBitset || ((requestedBitset & entityBitset) == requestedBitset));
            }
            else
            {// If we only have one requested ComponentType, we can skip the ComponentTypes bitset construction and test just the corresponding bit.
                typedef typename Meta::GetNth<0, ComponentTypes...>::Type ComponentType;
                const auto [archetype, index] = *mEntityToArchetypeID[pEntity.ID];
                return mArchetypes[archetype].mBitset.test(ComponentIDGenerator::get<ComponentType>());
            }
        }
    };
} // namespace ECS