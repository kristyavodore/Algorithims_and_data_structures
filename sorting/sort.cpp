#include <iostream>
#include <functional>
#include <vector>
#include <algorithm>

template <typename T>
class sorting{
public:
    virtual std::vector<T> sort(std::vector <T> const &vector, std::function<bool(T const &, T const &)> const & compar) const = 0;
    virtual ~sorting()= default;
};

template <typename T>
class counting_sorting final : public sorting <T>{
public:
    std::vector<T> sort(std::vector<T> const &vector, std::function< bool(T const &, T const &)> const & compar) const override{

        int c = vector.size();
        std::vector<int> count(c, 0);

        for (int i = c-1; i >= 1; --i ){
            for (int j = i-1; j >= 0; --j){
                if ( compar(vector[i], vector[j]) ){
                    ++count[j];
                }
                else{
                    ++count[i];
                }
            }
        }

        std::vector<T> result(c);
        for (int i = 0; i < c; ++i ){
            result[count[i]] = vector[i];
        }
        return result;
    }

};

template <typename T>
class inserting_sorting final : public sorting <T>{
public:
    std::vector<T> sort(std::vector<T> const & vector, std::function<bool(T const &, T const &)> const & compar) const override{

        int c = vector.size();
        for (int j = 1; j < c; ++j ){
            int key = vector[j];
            int i = j - 1;

            while ( i >= 0 && vector[i] > key )
            {
                vector[i+1] = vector[i];
                --i;
            }
            vector [i+1] = key;
        }
        return vector;
    }

};

template <typename T>
class selection_sorting final : public sorting<T>{

public:

    std::vector<T> sort(std::vector<T> const & vec, std::function <bool(T const &, T const &)> const & compar) override{

        std::vector<T> vector = vec;

        int c = vector.size();
        for (int j = c-1; j > 0; --j ){

            //находим макс элемент
            int max_position = j;
            int max = 0;
            for (int i = j - 1 ; i > 0 ; --i ){
                if ( compar(max, vector[i]) ){
                    max = vector[i];
                    max_position = i;
                }
            }

            if (j != max_position){
                std::swap(vector[j], vector[max_position]);
            }
        }
        return vec;
    }
};

template <typename T>
class shell_sorting final : public sorting<T> {

public:

    std::vector<T> sort(std::vector<T> const &vec, std::function<bool(T const &, T const &)> const &compar) override {

        std::vector<T> vector = vec;

        for (int h = vec.size() / 2; h > 0; h /= 2) {
            for (int j = h; j < vec.size(); ++j) {
                T k = vec[j];
                int i = j - h;

                while (i >= 0 && compar(k, vec[i])) {
                    vec[i + h] = vec[i];
                    i -= h;
                }

                vec[i + h] = k;
            }
        }
        return vec;
    }
};


int main() {
    std::vector<int> vec = { 9, 2, 4, 1, 5, 6, 3 };

    std::function<bool(const int&, const int&)> comp = [](const int& a, const int& b) { return a < b; };
    selection_sorting <int> s;
    std::vector<int> result = s.sort(vec, comp);

    for (auto const & i : result){
        std::cout << i << " ";
    }


    return 0;
}