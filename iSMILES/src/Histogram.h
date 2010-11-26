#pragma once
#include <vector>
#include <map>

namespace gga
{
    template <class index_t, class value_t> class Histogram
    {
        typedef std::vector<index_t> IndexVector;
        typedef std::map<value_t, IndexVector> ValueMap;
        
        ValueMap Data;
    public:
        Histogram()
        {
        }
        
        /* concatenate all groups with values differs less than delta into one group */
        Histogram<index_t, value_t> regroup(value_t Delta) const
        {
            Histogram<index_t, value_t> result;
            const size_t size = Data.size();
            
            if (empty())
                return result;
            
            std::vector<bool> processed(size);
            for (size_t u = 0; u < size; u++)
                processed[u] = false;
            
            while (true)
            {
                size_t best_count = 0;
                typename ValueMap::const_iterator best_center = Data.end();
                
                { // select first unprocessed as best_center                    
                    size_t u = 0;
                    for (typename ValueMap::const_iterator it = Data.begin(); it != Data.end(); it++, u++)
                    {
                        if (processed[u])
                            continue;
                            
                        best_center = it;
                        break;
                    }
                }
                
                if (best_center == Data.end())
                    break; // all processed
                
                { // maybe some other unprocessed element will be better as center
                    size_t u = 0;
                    for (typename ValueMap::const_iterator center = Data.begin(); center != Data.end(); center++, u++)
                    {
                        if (processed[u])
                            continue;

                        size_t count = 0;
                        
                        size_t u2 = 0;
                        for (typename ValueMap::const_iterator it = Data.begin(); it != Data.end(); it++, u2++)
                            if (!processed[u2] &&
                                it != center && 
                                fabs(it->first - center->first) < Delta)
                                    count += it->second.size();

                        if (count > best_count)
                        {
                            best_count = count;
                            best_center = center;
                        }
                    }
                }
                
                value_t center_value = best_center->first;
                size_t u = 0;
                for (typename ValueMap::const_iterator it = Data.begin(); it != Data.end(); it++, u++)
                {
                    if (!processed[u] && 
                        fabs(it->first - center_value) < Delta)
                    {
                        for (typename IndexVector::const_iterator idx = it->second.begin(); idx != it->second.end(); idx++)
                            result.add(*idx, center_value);
                        processed[u] = true;
                    }
                }
            }
            
            return result;
        }
        
        /* select only groups with members count more than average */
        Histogram<index_t, value_t> onlyRepresentative() const
        {
            Histogram<index_t, value_t> result;
            
            if (empty())
                return result;            

            size_t count = 0;
            for (typename ValueMap::const_iterator it = Data.begin(); it != Data.end(); it++)
                count += it->second.size();
            size_t average = count / groups();

            for (typename ValueMap::const_iterator it = Data.begin(); it != Data.end(); it++)
                if (it->second.size() >= average)
                    for (typename IndexVector::const_iterator idx = it->second.begin(); idx != it->second.end(); idx++)
                        result.add(*idx, it->first);
            return result;
        }
        
        /* add value for specified index */
        void add(index_t index, value_t value)
        {
            Data[value].push_back(index);
        }
        
        /* real average value */
        value_t average() const
        {
            value_t sum = 0;
            size_t count = 0;
            for (typename ValueMap::const_iterator it = Data.begin(); it != Data.end(); it++)
            {
                sum += it->first * it->second.size();
                count += it->second.size();
            }
            return sum / (value_t)count;
        }
        
        /* maximal difference */
        value_t range() const
        {
            value_t min = Data.begin()->first, max = min;
            for (typename ValueMap::const_iterator it = Data.begin(); it != Data.end(); it++)
            {
                if (it->first > max) max = it->first;
                if (it->first < min) min = it->first;
            }
            return max - min;
        }
        
        /* unique groups count */
        size_t groups() const
        {
            return Data.size();
        }
        
        /* index of each group start */
        IndexVector startIndexes() const
        {
            IndexVector result;
            for (typename ValueMap::const_iterator it = Data.begin(); it != Data.end(); it++)
                result.push_back(it->second.at(0));
            return result;
        }
        
        /* is collection empty */
        bool empty() const
        {
            return Data.empty();
        }
        
        void dump() const
        {
            for (typename ValueMap::const_iterator it = Data.begin(); it != Data.end(); it++)
                printf("  %f : %i\n", it->first, it->second.size());
        }
    };
}