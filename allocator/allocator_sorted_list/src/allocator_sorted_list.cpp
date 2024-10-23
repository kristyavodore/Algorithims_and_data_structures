#include <not_implemented.h>
#include <mutex>

#include "../include/allocator_sorted_list.h"

allocator_sorted_list::~allocator_sorted_list()
{
    if (_trusted_memory == nullptr)
    {
        return;
    }

    obtain_synchronizer().~mutex();
    //allocator::destruct(&obtain_synchronizer());

    debug_with_guard("Destructor");
    deallocate_with_guard(_trusted_memory);
}

allocator_sorted_list::allocator_sorted_list(
    allocator_sorted_list &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
    debug_with_guard("Move constructor");
}

allocator_sorted_list &allocator_sorted_list::operator=(
    allocator_sorted_list &&other) noexcept
{
    if (this != &other)
    {
        _trusted_memory = nullptr;
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    debug_with_guard("Move assignment operator");
    return *this;
}

allocator_sorted_list::allocator_sorted_list(
    size_t space_size,
    allocator *parent_allocator, // parent_allocator - указатель на место, откуда выделяем память для аллокатора
    logger *logger,
    allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size < available_block_metadata_size()) // размер памяти под аллокатор без его меты меньше, чем мета свободного блока
    {
        error_with_guard("Can't initialize allocator instance");
        throw std::logic_error("Can't initialize allocator instance");
    }

    size_t memory_size = space_size + summ_size(); // сложили память под аллокатор и его мету
    try
    {
        _trusted_memory = parent_allocator == nullptr
                          ? ::operator new (memory_size)
                          : parent_allocator->allocate(1, memory_size);
    }
    catch (std::bad_alloc const &ex)
    {
        error_with_guard(ex.what());
        throw ex;
    }

    allocator **parent_allocator_placement = reinterpret_cast<allocator **>(_trusted_memory);
    *parent_allocator_placement = parent_allocator;

    class logger **logger_placement = reinterpret_cast<class logger **>(parent_allocator_placement + 1);
    *logger_placement = logger;

    std::mutex *synchronizer_placement = reinterpret_cast<std::mutex *>(logger_placement + 1);
    new (reinterpret_cast<void *>(synchronizer_placement)) std::mutex();
    // allocator::construct(synchronizer_placement);

    //unsigned char *placement = reinterpret_cast<unsigned char *>(synchronizer_placement);

    allocator_with_fit_mode::fit_mode *fit_mode_placement = reinterpret_cast<allocator_with_fit_mode::fit_mode *>(synchronizer_placement+1);
    *fit_mode_placement = allocate_fit_mode;

    size_t *size_placement = reinterpret_cast<size_t *>(fit_mode_placement+1);
    *size_placement =  space_size;

    void **pointer_to_list_placement = reinterpret_cast<void **>(size_placement+1);
    *pointer_to_list_placement = pointer_to_list_placement+1;

    *reinterpret_cast<void **>(pointer_to_list_placement+1) = nullptr;


}

[[nodiscard]] void *allocator_sorted_list::allocate(
    size_t value_size,
    size_t values_count)
{
    std::lock_guard<std::mutex> lock(obtain_synchronizer());

    void * target_block = nullptr, *previous_to_target_block = nullptr;
    size_t requested_size = value_size * values_count + ancillary_block_metadata_size();

    size_t target_block_size;

    void *current_block, *previous_block = nullptr;
    current_block = obtain_first_available_block_address();
    allocator_with_fit_mode::fit_mode fit_mode = obtain_fit_mode();

    while (current_block != nullptr) {
        size_t current_block_size = obtain_available_block_size(current_block);
        if (current_block_size >= requested_size && (
                fit_mode == allocator_with_fit_mode::fit_mode::first_fit ||
                (fit_mode == allocator_with_fit_mode::fit_mode::the_best_fit &&
                 (target_block == nullptr || current_block_size < target_block_size)) ||
                (fit_mode == allocator_with_fit_mode::fit_mode::the_worst_fit &&
                 (target_block == nullptr || current_block_size > target_block_size)))) {
            target_block = current_block;
            target_block_size = current_block_size;
            previous_to_target_block = previous_block;

            if (fit_mode == allocator_with_fit_mode::fit_mode::first_fit) {
                break;
            }

            previous_block = current_block;
            current_block = obtain_next_available_block_address(current_block);
        }
    }

    if (target_block == nullptr)
    {
        error_with_guard("target_block is nullptr");
        throw std::bad_alloc();
    }



    if (values_count * values_count + available_block_metadata_size() >= obtain_available_block_size (target_block)) // если заполнения блока в него влезает мета свободного
    {
        obtain_available_block_size (target_block) = values_count * values_count; // в мету найдённого блока кладём запрошенный размер

        void ** placement_available_piece = reinterpret_cast<void**>(&obtain_available_block_size(target_block) + obtain_available_block_size (target_block));
        *placement_available_piece = obtain_next_available_block_address(target_block);
        obtain_available_block_size(placement_available_piece) = obtain_available_block_size (target_block) - values_count * values_count - available_block_metadata_size();

        //*reinterpret_cast<void**>(&obtain_available_block_size(target_block) + obtain_available_block_size (target_block)) = obtain_next_available_block_address(target_block); // в оставшуюся часть от свободного блока положили указатель на след свободный (то есть то, что до этого было у target_block в поле указатель)
        // в поле размер у оставшегося кусочка кладём: размер бывш своб блока - запрошенный - мета свободного:
        //obtain_available_block_size(*reinterpret_cast<void**>(&obtain_available_block_size(target_block) + obtain_available_block_size (target_block))) = obtain_available_block_size (target_block) - values_count * values_count - available_block_metadata_size();

        obtain_next_available_block_address(previous_to_target_block) = placement_available_piece;
    }
    else
    {
        // там уже и так лежит этот размер вроде
        obtain_next_available_block_address(previous_to_target_block) = obtain_next_available_block_address(obtain_next_available_block_address(target_block)); // в поле указателя из previous_to_target_block кладём указатель на следующий после target
    }

    obtain_next_available_block_address(target_block) = _trusted_memory;

    return reinterpret_cast<void *>(reinterpret_cast<unsigned char *>(target_block) + ancillary_block_metadata_size());
}

void allocator_sorted_list::deallocate(
    void *at)
{
    throw not_implemented("void allocator_sorted_list::deallocate(void *)", "your code should be here...");
}

inline void allocator_sorted_list::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    obtain_fit_mode() = mode;
}


inline allocator *allocator_sorted_list::get_allocator() const
{
    return *reinterpret_cast<allocator**>(_trusted_memory);
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept
{
    //TODO this
    throw not_implemented("std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept", "your code should be here...");
}

inline logger *allocator_sorted_list::get_logger() const
{
    return *reinterpret_cast<logger **>(reinterpret_cast<void**>(_trusted_memory)+1);
}

inline std::string allocator_sorted_list::get_typename() const noexcept
{
    return "allocator_sorted_list";
}

size_t allocator_sorted_list::available_block_metadata_size()
{
    return sizeof(size_t) + sizeof(void *);
}

size_t allocator_sorted_list::ancillary_block_metadata_size()
{
    return sizeof(void *) + sizeof(size_t);
}

size_t allocator_sorted_list::summ_size()
{
    return sizeof(allocator *) + sizeof(std::mutex) + sizeof(logger *) + sizeof( allocator_with_fit_mode::fit_mode) + sizeof(size_t) + sizeof(void *);
}

std::mutex &allocator_sorted_list::obtain_synchronizer() const
{
    return *reinterpret_cast<std::mutex *>(reinterpret_cast<unsigned char *>(_trusted_memory) + sizeof(allocator *) + sizeof(logger *));
}

allocator_with_fit_mode::fit_mode &allocator_sorted_list::obtain_fit_mode() const
{
    return *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(reinterpret_cast<unsigned char *>(_trusted_memory) + sizeof(allocator *) + sizeof(logger *) + sizeof(std::mutex));
}



void* allocator_sorted_list::obtain_first_available_block_address()
{
    return *reinterpret_cast<void **>(reinterpret_cast<unsigned char *>(_trusted_memory) + sizeof(allocator *) + sizeof(logger *) + sizeof(std::mutex) + sizeof(allocator_with_fit_mode::fit_mode) + sizeof(size_t));
}

void *&allocator_sorted_list::obtain_next_available_block_address(
        void *current_available_block_address){
    return *reinterpret_cast<void **>(current_available_block_address);
}

size_t & allocator_sorted_list::obtain_available_block_size(void * current_block){
    return *reinterpret_cast<size_t *>(&obtain_next_available_block_address(current_block)+1);
}


// Вопросы
// Что ещё дописать в конструктор перемещения?
// Зачем вызывать deallocate_with_guard в деструкторе и как он работает?
// set_fit_mode() - можно ли сразу положить в obtain_fit_mode()?