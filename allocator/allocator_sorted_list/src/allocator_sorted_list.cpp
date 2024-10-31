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
        error_with_guard(std::string(ex.what()) + " no memory allocated for _trusted_memory");
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

    *reinterpret_cast<size_t *>(reinterpret_cast<void **>(pointer_to_list_placement)+1) = space_size - available_block_metadata_size();

    debug_with_guard("Constructor");

}

[[nodiscard]] void *allocator_sorted_list::allocate(
    size_t value_size,
    size_t values_count)
{
    std::lock_guard<std::mutex> lock(obtain_synchronizer());

    if (_trusted_memory == nullptr)
    {
        error_with_guard("Allocator instance state was moved :/");
        throw std::logic_error("Allocator instance state was moved :/");
    }

    debug_with_guard("Start allocate");

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


/*
    if (obtain_available_block_size (target_block) - values_count * value_size < available_block_metadata_size())
    {
        // если оставшееся место меньше меты свободного блока
        (previous_to_target_block != nullptr
         ? obtain_next_available_block_address(previous_to_target_block)  // в поле указателя из previous_to_target_block кладём указатель на следующий после target
         : obtain_first_available_block_address()) = obtain_next_available_block_address(target_block);

        //obtain_next_available_block_address(target_block) = _trusted_memory;
    }
    else
    {
        (previous_to_target_block != nullptr
         ? obtain_next_available_block_address(previous_to_target_block)
         : obtain_first_available_block_address()) = reinterpret_cast<void*>(reinterpret_cast<unsigned char *>(target_block) + requested_size);

        obtain_next_available_block_address(reinterpret_cast<unsigned char *>(target_block) + requested_size) = obtain_next_available_block_address(target_block);
    obtain_available_block_size(reinterpret_cast<unsigned char *>(target_block) + requested_size) = obtain_available_block_size (target_block) - requested_size + ancillary_block_metadata_size();

        obtain_available_block_size(target_block) =  values_count * value_size;
    }
*/


    if (obtain_available_block_size (target_block) - values_count * value_size >= available_block_metadata_size()) // если после заполнения блока в него влезает мета свободного
    {
        obtain_available_block_size (target_block) = values_count * value_size; // в мету найдённого блока кладём запрошенный размер

        void * placement_available_piece = reinterpret_cast<void*>(reinterpret_cast<unsigned char *>(target_block) + requested_size);
        obtain_next_available_block_address(placement_available_piece) = obtain_next_available_block_address(target_block);
        obtain_available_block_size(placement_available_piece) =
                obtain_available_block_size (target_block)
                - values_count * value_size
                - available_block_metadata_size();

        //*reinterpret_cast<void**>(&obtain_available_block_size(target_block) + obtain_available_block_size (target_block)) = obtain_next_available_block_address(target_block); // в оставшуюся часть от свободного блока положили указатель на след свободный (то есть то, что до этого было у target_block в поле указатель)
        // в поле размер у оставшегося кусочка кладём: размер бывш своб блока - запрошенный - мета свободного:
        //obtain_available_block_size(*reinterpret_cast<void**>(&obtain_available_block_size(target_block) + obtain_available_block_size (target_block))) = obtain_available_block_size (target_block) - values_count * value_size - available_block_metadata_size();

        (previous_to_target_block != nullptr
            ? obtain_next_available_block_address(previous_to_target_block)
            : obtain_first_available_block_address()) = placement_available_piece;

        //obtain_next_available_block_address(previous_to_target_block) = placement_available_piece;
    }
    else
    {
        warning_with_guard("more memory allocated than the user requested");
        // там уже и так лежит этот размер вроде
        (previous_to_target_block != nullptr
            ? obtain_next_available_block_address(previous_to_target_block)  // в поле указателя из previous_to_target_block кладём указатель на следующий после target
            : obtain_first_available_block_address()) = obtain_next_available_block_address(target_block);
    }


    obtain_next_available_block_address(target_block) = _trusted_memory;

    debug_with_guard("Finish allocate");

    return reinterpret_cast<void *>(reinterpret_cast<unsigned char *>(target_block) + ancillary_block_metadata_size());
}

void allocator_sorted_list::deallocate(
    void *at)
{
    std::lock_guard<std::mutex> lock(obtain_synchronizer());

    if (_trusted_memory == nullptr)
    {
        error_with_guard("Allocator instance state was moved :/");
        throw std::logic_error("Allocator instance state was moved :/");
    }

    debug_with_guard("Start deallocate");

    at = reinterpret_cast<void *>(reinterpret_cast<unsigned char *>(at) - ancillary_block_metadata_size());

    if (at == nullptr || at < (reinterpret_cast<unsigned char *>(_trusted_memory) + summ_size()) || at >  (reinterpret_cast<unsigned char *>(_trusted_memory) + summ_size() + obtain_trusted_memory_size() - ancillary_block_metadata_size()))
    {
        error_with_guard("Invalid block address");
        throw std::logic_error("Invalid block address");
    }

    if (obtain_trusted_memory_ancillary_block(at) != _trusted_memory)
    {
        error_with_guard("Attempt to deallocate block into wrong allocator instance");
        throw std::logic_error("Attempt to deallocate block into wrong allocator instance");
    }

    void * left_available_block = nullptr;
    void * right_available_block = obtain_first_available_block_address();

    while (right_available_block != nullptr && (at > left_available_block || left_available_block== nullptr) && at < right_available_block)
    {
        left_available_block = right_available_block;
        right_available_block = obtain_next_available_block_address(right_available_block);
    }

    if (left_available_block == nullptr && right_available_block == nullptr)
    {
        obtain_first_available_block_address() = at;
        obtain_next_available_block_address(at) = nullptr;

        return; // всё закончили, так как он единственный в списке
    }

    obtain_next_available_block_address(at) = right_available_block;

    (left_available_block == nullptr
     ? obtain_first_available_block_address()
     : obtain_next_available_block_address(left_available_block)) = at;

    // merge with right
    if (reinterpret_cast<void *>((reinterpret_cast<unsigned char *>(at) + obtain_available_block_size(at))) == right_available_block)
    {
        obtain_next_available_block_address(at) = obtain_next_available_block_address(right_available_block);
        obtain_available_block_size(at) = obtain_available_block_size(at) + available_block_metadata_size() + obtain_available_block_size(right_available_block);
    }

    // merge with left block
    if (reinterpret_cast<void *>(reinterpret_cast<unsigned char *>(left_available_block) + obtain_available_block_size(left_available_block)) == at)
    {
        obtain_next_available_block_address(left_available_block) = obtain_next_available_block_address(at);
        obtain_available_block_size(left_available_block) = obtain_available_block_size(left_available_block) + available_block_metadata_size() + obtain_available_block_size(at);

    }

    debug_with_guard("Finish deallocate");
}

inline void allocator_sorted_list::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    obtain_fit_mode() = mode;
    trace_with_guard("Method set_fit_mode()");
}


inline allocator *allocator_sorted_list::get_allocator() const
{
    trace_with_guard("Method get_allocator()");
    return *reinterpret_cast<allocator**>(_trusted_memory);
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept
{
    debug_with_guard("start method get_blocks_info()");

    std::vector<allocator_test_utils::block_info> all_blocks;
    allocator_test_utils::block_info block{}; //он хотел от меня странные скобочки:/

    void * Vrem_ptr = reinterpret_cast<void *>(reinterpret_cast<unsigned char *>(_trusted_memory) + summ_size());
//    size_t Vrem_block_size;
//    bool Vrem_is_block_occupied;

    while (Vrem_ptr != nullptr)
    {
        // true - занято, false - свободно
        if (*reinterpret_cast<void **>(Vrem_ptr) == _trusted_memory)
            block.is_block_occupied = true;
        if (*reinterpret_cast<void **>(Vrem_ptr) == reinterpret_cast<void *>(reinterpret_cast<unsigned char *>(Vrem_ptr) + obtain_available_block_size(Vrem_ptr) + available_block_metadata_size()))
            block.is_block_occupied = false;

        block.block_size =  *reinterpret_cast<size_t *>(reinterpret_cast<void **>(Vrem_ptr)+1); // размер

        all_blocks.push_back(block);

        Vrem_ptr = obtain_next_available_block_address(Vrem_ptr);
    }
    debug_with_guard("finish method get_blocks_info()");
    return all_blocks;
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



void*& allocator_sorted_list::obtain_first_available_block_address()
{
    return *reinterpret_cast<void **>(reinterpret_cast<unsigned char *>(_trusted_memory) + sizeof(allocator *) + sizeof(logger *) + sizeof(std::mutex) + sizeof(allocator_with_fit_mode::fit_mode) + sizeof(size_t));
}

void *&allocator_sorted_list::obtain_next_available_block_address(
        void *current_available_block_address){
    return *reinterpret_cast<void **>(current_available_block_address);
}

void *&allocator_sorted_list::obtain_trusted_memory_ancillary_block(
        void *current_available_block_address)
{
    return obtain_next_available_block_address(current_available_block_address);
}


size_t & allocator_sorted_list::obtain_available_block_size(void * current_block){
    return *reinterpret_cast<size_t *>(&obtain_next_available_block_address(current_block)+1);
}

size_t &allocator_sorted_list::obtain_trusted_memory_size() const
{
    return *reinterpret_cast<size_t *>(&obtain_fit_mode() + 1);
}

// Вопросы
// Что ещё дописать в конструктор перемещения?
// Зачем вызывать deallocate_with_guard в деструкторе и как он работает?
// set_fit_mode() - можно ли сразу положить в obtain_fit_mode()?


//    std::vector<allocator_test_utils::block_info> all_blocks = reinterpret_cast<allocator_sorted_list*>(alloc) -> get_blocks_info();
//
//    for (auto const &iter: all_blocks)
//    {
//        std::cout << iter.block_size << iter.is_block_occupied << std::endl;
//    }