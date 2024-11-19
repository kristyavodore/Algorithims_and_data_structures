#include <not_implemented.h>
#include "../include/allocator_boundary_tags.h"

allocator_boundary_tags::~allocator_boundary_tags()
{
    debug_with_guard("Destructor");
    free_memory();
}

// Конструктора копирования и оператора присваивания копированием НЕТ И НЕ БУДЕТ!!

allocator_boundary_tags::allocator_boundary_tags(
    allocator_boundary_tags &&other) noexcept
{
    std::lock_guard<std::mutex> locck(other.obtain_mutex()); // переменная специально названа криво!!
    //std::lock_guard<std::mutex> lock(other.obtain_synchronizer());

    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    debug_with_guard("Move constructor");
}

allocator_boundary_tags &allocator_boundary_tags::operator=(
    allocator_boundary_tags &&other) noexcept
{
    if (this != &other)
    {
        std::lock_guard<std::mutex> locck(other.obtain_mutex());
        // очистить память?? Да, надо



        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    debug_with_guard("Move assignment operator");
    return *this;
}

allocator_boundary_tags::allocator_boundary_tags(
    size_t space_size,
    allocator *parent_allocator,
    logger *logger,
    allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size < block_metadata_size()*2)
    {
        error_with_guard("Can't initialize allocator instance");
        throw std::logic_error("Can't initialize allocator instance");
    }

    size_t all_size = space_size + common_metadata_size() + double_block_metadata_size();

    try{
        _trusted_memory = parent_allocator != nullptr
                          ? parent_allocator->allocate(1, all_size)
                          : ::operator new(all_size);
    }
    catch (std::bad_alloc const &ex) {
        error_with_guard(std::string(ex.what()) + " no memory allocated for _trusted_memory");
        throw ex;
    }

    allocator** placement_parent_allocator = reinterpret_cast<allocator **>(_trusted_memory);
    *placement_parent_allocator = parent_allocator;

    class::logger ** placement_logger = reinterpret_cast<class::logger **>(placement_parent_allocator+1);
    *placement_logger = logger;

    std::mutex *placement_mutex = reinterpret_cast<std::mutex *>(placement_logger + 1);
    //new (placement_mutex) std::mutex();
    construct(placement_mutex);

    allocator_with_fit_mode::fit_mode *placement_fit_mode = reinterpret_cast<allocator_with_fit_mode::fit_mode *>(placement_mutex + 1);
    *placement_fit_mode = allocate_fit_mode;

    size_t *placement_space_size = reinterpret_cast<size_t *>(placement_fit_mode + 1);
    *placement_space_size = space_size + double_block_metadata_size();

    //мета свободного блока
    // |размер|bool|указ на _trusted_memory|      |размер|bool|указ на _trusted_memory|
    // размер указан С УЧЁТОМ размера двух мет свободного блока
    size_t *placement_block_size_1 = reinterpret_cast<size_t *>(placement_space_size + 1);
    *placement_block_size_1 = space_size + double_block_metadata_size();

    bool *placement_block_is_occupied_1 = reinterpret_cast<bool *>(placement_block_size_1 + 1);
    *placement_block_is_occupied_1 = false;

    void ** placement_trusted_memory_ptr_1 = reinterpret_cast<void **>(placement_block_is_occupied_1+1);
    *placement_trusted_memory_ptr_1 = _trusted_memory;

    size_t *placement_block_size_2 = reinterpret_cast<size_t *>(reinterpret_cast<unsigned char *>(placement_block_size_1) + space_size - block_metadata_size());
    *placement_block_size_2 = space_size + double_block_metadata_size();

    bool *placement_block_is_occupied_2 = reinterpret_cast<bool *>(reinterpret_cast<unsigned char *>(placement_block_is_occupied_1) + space_size - block_metadata_size());
    *placement_block_is_occupied_2 = false;

    void ** placement_trusted_memory_ptr_2 = reinterpret_cast<void**>(reinterpret_cast<unsigned char *>(placement_trusted_memory_ptr_1) + space_size - block_metadata_size());
    *placement_trusted_memory_ptr_2 = _trusted_memory;

    print_log_get_blocks_info();

    debug_with_guard("Constructor");
}

[[nodiscard]] void *allocator_boundary_tags::allocate(
    size_t value_size,
    size_t values_count)
{
    std::lock_guard<std::mutex> lock(obtain_mutex());

    if (_trusted_memory == nullptr)
    {
        error_with_guard("Allocator instance state was moved :/");
        throw std::logic_error("Allocator instance state was moved :/");
    }

    debug_with_guard("Start allocate");

    size_t requested_size = value_size * values_count + double_block_metadata_size();
    void * target_block = nullptr;
    size_t target_block_size;

    void * current_block = ptr_on_first_block();

    allocator_with_fit_mode::fit_mode fit_mode = obtain_fit_mode();

    while(chek_ptr_in_allowed_area(current_block)) {
        if ( !obtain_is_block_occupied(current_block) && obtain_size_block(current_block) >= requested_size &&
                (fit_mode == allocator_with_fit_mode::fit_mode::first_fit ||
                (fit_mode == allocator_with_fit_mode::fit_mode::the_best_fit
                && (target_block == nullptr || obtain_size_block(current_block) < target_block_size)) ||
                (fit_mode == allocator_with_fit_mode::fit_mode::the_worst_fit
                && (target_block == nullptr || obtain_size_block(current_block) > target_block_size))))
        {
            target_block = current_block;
            target_block_size = obtain_size_block(current_block);

            if (fit_mode == allocator_with_fit_mode::fit_mode::first_fit) {
                break;
            }

        }
        current_block = next_block(current_block);
    }

    if (target_block == nullptr)
    {
        error_with_guard("target_block is nullptr");
        throw std::bad_alloc();
    }


    if (target_block_size - requested_size >= double_block_metadata_size())
    {
        // если в блок влезает ещё кусочек (т.е. есть ещё место для двойной меты свободного блока)
        void * placement_available_piece = reinterpret_cast<void *>(reinterpret_cast<unsigned  char*>(target_block) + requested_size);
        // заполняем мету кусочка:
        set_size_block(placement_available_piece, target_block_size - requested_size);
        set_bool_block(placement_available_piece, false);
        set_void_block(placement_available_piece);

        //заполняем мету выделенного для пользователя блока
        set_size_block(target_block, requested_size);
        set_bool_block(target_block, true);
        set_void_block(target_block);
    }

    else
    {
        // если в блок не помещается больше ничего..((
        warning_with_guard("more memory allocated than the user requested");
        set_bool_block(target_block, true);
        // размер и указ на _trusted_memory там и так лежат вроде...
    }

    debug_with_guard("Finish allocate");
    print_log_get_blocks_info();
    return reinterpret_cast<void *>(reinterpret_cast<unsigned char*>(target_block) + block_metadata_size());
}

void allocator_boundary_tags::deallocate(
    void *at)
{
    std::lock_guard<std::mutex> lock(obtain_mutex());

    if (_trusted_memory == nullptr)
    {
        error_with_guard("Allocator instance state was moved :/");
        throw std::logic_error("Allocator instance state was moved :/");
    }

    debug_with_guard("Start deallocate");

    at = reinterpret_cast<void *>(reinterpret_cast<unsigned char*>(at) - block_metadata_size());

    if (at == nullptr|| reinterpret_cast<unsigned char *>(_trusted_memory) + common_metadata_size() > at
        || at > reinterpret_cast<unsigned char *>(_trusted_memory) + common_metadata_size() + obtain_space_size() - block_metadata_size())
    {
        error_with_guard("Invalid block address");
        throw std::logic_error("Invalid block address");
    }

    set_bool_block(at, false); // типо освобожаем

    //merge with right
    if (!obtain_is_block_occupied(next_block(at)) && chek_ptr_in_allowed_area(next_block(at)))
    {
        set_size_block(at, obtain_size_block(at) + obtain_size_block(next_block(at)));
    }

    //merge with left
    if (!obtain_is_block_occupied(reinterpret_cast<unsigned char*>(at) - block_metadata_size()) && chek_ptr_in_allowed_area(reinterpret_cast<void *>(reinterpret_cast<unsigned char*>(at) - block_metadata_size())))
    {
        void* p_left = reinterpret_cast<void *>(reinterpret_cast<unsigned char*>(at) - obtain_size_block(reinterpret_cast<unsigned char*>(at) - block_metadata_size()));
        set_size_block(p_left, obtain_size_block(p_left) + obtain_size_block(at));
    }

    /*void * p_left = nullptr;
    void * p_right = ptr_on_first_block();
    while (chek_ptr_in_allowed_area(p_right))
    {
        if ((p_left == nullptr || p_left < at) && at < p_right){
            break;
        }
        p_left = p_right;
        p_right = next_block(p_right);
    }

    set_bool_block(at, false); // типо освобожаем

    if (p_left == nullptr && p_right == nullptr)
    {
        return; // он единственный свободный блок
    }

    // merge with right
    if (p_right != nullptr && !obtain_is_block_occupied(p_right) && next_block(at) == p_right)
    {
        set_size_block(at, obtain_size_block(p_right) + obtain_size_block(at));
    }

    // merge with left
    if (p_left != nullptr && !obtain_is_block_occupied(p_left) && next_block(p_left) == at)
    {
        set_size_block(p_left, obtain_size_block(p_left) + obtain_size_block(at));
    }*/

    debug_with_guard("Finish deallocate");
    print_log_get_blocks_info();
}

inline void allocator_boundary_tags::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    obtain_fit_mode() = mode;
    trace_with_guard("Method set_fit_mode()");
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const noexcept
{
    std::vector<allocator_test_utils::block_info> all_blocks;
    allocator_test_utils::block_info block{};

    void * Vrem_ptr = ptr_on_first_block();
    while (chek_ptr_in_allowed_area(Vrem_ptr))
    {
        block.is_block_occupied = obtain_is_block_occupied(Vrem_ptr);
        block.block_size = obtain_size_block(Vrem_ptr);
        all_blocks.push_back(block);

        Vrem_ptr = next_block(Vrem_ptr);
    }
    return all_blocks;
}

void allocator_boundary_tags::print_log_get_blocks_info()
{
    std::vector<allocator_test_utils::block_info> all_blocks = get_blocks_info();
    std::string result_string;

    for (auto const &iter : all_blocks)
    {
        result_string += "|";
        result_string += iter.is_block_occupied ? "<occup>" : "<avail>";
        result_string += "<" + std::to_string(iter.block_size) + ">";
        result_string += "|";
    }
    debug_with_guard(result_string);
}

inline allocator *allocator_boundary_tags::get_allocator() const
{
    return *reinterpret_cast<allocator **>(_trusted_memory);
}

inline logger *allocator_boundary_tags::get_logger() const
{
    return *reinterpret_cast<logger **>(reinterpret_cast<unsigned char *>(_trusted_memory) + sizeof(allocator*));
}

std::mutex&  allocator_boundary_tags::obtain_mutex() const
{
    return *reinterpret_cast<std::mutex *>(reinterpret_cast<unsigned char *>(_trusted_memory) + sizeof(allocator*) + sizeof(logger *));
}

allocator_with_fit_mode::fit_mode &allocator_boundary_tags::obtain_fit_mode() const
{
    return *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(reinterpret_cast<unsigned char *>(_trusted_memory) + sizeof(allocator *) + sizeof(logger *) + sizeof(std::mutex));
}

size_t allocator_boundary_tags::obtain_space_size() const
{
    return *reinterpret_cast<size_t *>(reinterpret_cast<unsigned char *>(_trusted_memory) + sizeof(allocator*) + sizeof(logger *) + +  sizeof(std::mutex) + sizeof( allocator_with_fit_mode::fit_mode));
}

inline std::string allocator_boundary_tags::get_typename() const noexcept
{
    return "allocator_boundary_tags";
}

size_t allocator_boundary_tags::block_metadata_size(){
    return sizeof(size_t) + sizeof(bool) + sizeof(void *);
}

size_t allocator_boundary_tags::double_block_metadata_size(){
    return block_metadata_size()*2;
}

size_t allocator_boundary_tags::common_metadata_size(){
    return sizeof(allocator *) + sizeof(logger *) +  sizeof(std::mutex) + sizeof( allocator_with_fit_mode::fit_mode) + sizeof(size_t);
}

void * allocator_boundary_tags::ptr_on_first_block() const{
    return reinterpret_cast<void*>(reinterpret_cast<unsigned char *>(_trusted_memory) + common_metadata_size());
}

bool allocator_boundary_tags::chek_ptr_in_allowed_area(void * current_block) const{
    if (reinterpret_cast<unsigned char *>(ptr_on_first_block()) <= reinterpret_cast<void *>(current_block) && reinterpret_cast<void *>(current_block) <=
                                                                                    reinterpret_cast<unsigned char *>(ptr_on_first_block()) +
                                                                                    obtain_space_size() -
                                                                                    block_metadata_size())
        return true;
    else
        return false;
}

//bool allocator_boundary_tags::chek_ptr_in_allowed_area(void * current_block) const{
//    size_t trusted_memory_size = obtain_space_size() + common_metadata_size();
//    return (reinterpret_cast<unsigned char *>(current_block) < reinterpret_cast<unsigned char *>(_trusted_memory) + trusted_memory_size);
//}

size_t &allocator_boundary_tags::obtain_size_block(void *current_block){
    return *reinterpret_cast<size_t *>(current_block);
}

bool allocator_boundary_tags::obtain_is_block_occupied(void *current_block){
    return *reinterpret_cast<bool *>(reinterpret_cast<unsigned char *>(current_block) + sizeof (size_t));
}

void * allocator_boundary_tags::next_block(void * current_block){
    return reinterpret_cast<void*>(reinterpret_cast<unsigned char *>(current_block) + obtain_size_block(current_block));
}

void allocator_boundary_tags::set_size_block(void * current_block, size_t size_block){
    *reinterpret_cast<size_t *>(current_block) = size_block;
    *reinterpret_cast<size_t *>(reinterpret_cast<unsigned char *>(current_block) + size_block - block_metadata_size()) = size_block;
}

void allocator_boundary_tags::set_bool_block(void * current_block, bool bool_block) {
    *reinterpret_cast<bool *>(reinterpret_cast<unsigned char *>(current_block) + sizeof (size_t)) = bool_block;
    *reinterpret_cast<bool *>(reinterpret_cast<unsigned char *>(current_block) + obtain_size_block(current_block) - block_metadata_size() + sizeof (size_t)) = bool_block;
}

void allocator_boundary_tags::set_void_block(void * current_block) {
    *reinterpret_cast<void **>(reinterpret_cast<unsigned char *>(current_block) + sizeof (size_t) + sizeof (bool)) = _trusted_memory;
    *reinterpret_cast<void **>(reinterpret_cast<unsigned char *>(current_block) + obtain_size_block(current_block) - block_metadata_size() + sizeof (size_t) + sizeof (bool)) = _trusted_memory;
}

void allocator_boundary_tags::free_memory(){
    if (_trusted_memory == nullptr){
        return;
    }
    debug_with_guard("free_memory()");
    allocator::destruct(&obtain_mutex());

    deallocate_with_guard(_trusted_memory);
}