#ifndef _SZ_INTERPOLATION_COMPRESSSOR_HPP
#define _SZ_INTERPOLATION_COMPRESSSOR_HPP

#include "QoZ/predictor/Predictor.hpp"
#include "QoZ/predictor/LorenzoPredictor.hpp"
#include "QoZ/quantizer/Quantizer.hpp"
#include "QoZ/encoder/Encoder.hpp"
#include "QoZ/lossless/Lossless.hpp"
#include "QoZ/utils/Iterator.hpp"
#include "QoZ/utils/MemoryUtil.hpp"
#include "QoZ/utils/Config.hpp"
#include "QoZ/utils/FileUtil.hpp"
#include "QoZ/utils/Interpolators.hpp"
#include "QoZ/utils/Timer.hpp"
#include "QoZ/def.hpp"
#include "QoZ/utils/Config.hpp"
#include <cstring>
#include <cmath>
#include <limits>
namespace QoZ {
    template<class T, uint N, class Quantizer, class Encoder, class Lossless>
    class SZInterpolationCompressor {
    public:


        SZInterpolationCompressor(Quantizer quantizer, Encoder encoder, Lossless lossless) :
                quantizer(quantizer), encoder(encoder), lossless(lossless) {

            static_assert(std::is_base_of<concepts::QuantizerInterface<T>, Quantizer>::value,
                          "must implement the quatizer interface");
            static_assert(std::is_base_of<concepts::EncoderInterface<int>, Encoder>::value,
                          "must implement the encoder interface");
            static_assert(std::is_base_of<concepts::LosslessInterface, Lossless>::value,
                          "must implement the lossless interface");
        }

        T *decompress(uchar const *cmpData, const size_t &cmpSize, size_t num) {
            T *dec_data = new T[num];
            return decompress(cmpData, cmpSize, dec_data);
        }

        T *decompress(uchar const *cmpData, const size_t &cmpSize, T *decData) {

            size_t remaining_length = cmpSize;
            uchar *buffer = lossless.decompress(cmpData, remaining_length);
            int levelwise_predictor_levels;
            int blockwiseTuning;
            uchar const *buffer_pos = buffer;
            std::vector <uint8_t> interpAlgo_list;
            std::vector <uint8_t> interpDirection_list;
            int fixBlockSize;
            //size_t maxStep;
           
            read(global_dimensions.data(), N, buffer_pos, remaining_length);
           
            read(blocksize, buffer_pos, remaining_length);
            
            read(interpolator_id, buffer_pos, remaining_length);
            
            read(direction_sequence_id, buffer_pos, remaining_length);
           
            read(alpha,buffer_pos,remaining_length);
            
            read(beta,buffer_pos,remaining_length);
           
            read(maxStep,buffer_pos,remaining_length);
           
            read(levelwise_predictor_levels,buffer_pos);
            
            read(blockwiseTuning,buffer_pos);

            read(fixBlockSize,buffer_pos);
            size_t cross_block=0;
            read(cross_block,buffer_pos);
            if(blockwiseTuning){
                size_t ops_num;
                read(ops_num,buffer_pos);
                interpAlgo_list=std::vector <uint8_t>(ops_num,0);
                interpDirection_list=std::vector <uint8_t>(ops_num,0);
                read(interpAlgo_list.data(),ops_num,buffer_pos);
                read(interpDirection_list.data(),ops_num,buffer_pos);



            }

            else if(levelwise_predictor_levels>0){
                interpAlgo_list=std::vector <uint8_t>(levelwise_predictor_levels,0);
                interpDirection_list=std::vector <uint8_t>(levelwise_predictor_levels,0);
                read(interpAlgo_list.data(),levelwise_predictor_levels,buffer_pos);
                read(interpDirection_list.data(),levelwise_predictor_levels,buffer_pos);
            }
            

            init();
            //QoZ::Timer timer(true);
            quantizer.load(buffer_pos, remaining_length);
            encoder.load(buffer_pos, remaining_length);
            quant_inds = encoder.decode(buffer_pos, num_elements);

            encoder.postprocess_decode();

            lossless.postdecompress_data(buffer);
            //timer.stop("decode");
            //timer.start();
            double eb = quantizer.get_eb();
            if(!anchor){
                *decData = quantizer.recover(0, quant_inds[quant_index++]);
            }
            
            else{

             
                 recover_grid(decData,global_dimensions,maxStep);
                    
                interpolation_level--;

                
            }
            size_t op_index=0;


    
            for (uint level = interpolation_level; level > 0 && level <= interpolation_level; level--) {
                if (alpha<0) {
                    if (level >= 3) {
                        quantizer.set_eb(eb * eb_ratio);
                    } else {
                        quantizer.set_eb(eb);
                    }
                }
                
                else if (alpha>=1){
                    
                    
                    double cur_ratio=pow(alpha,level-1);
                    if (cur_ratio>beta){
                        cur_ratio=beta;
                    }
                    
                    quantizer.set_eb(eb/cur_ratio);
                }
                else{
                    
                    
                    double cur_ratio=1-(level-1)*alpha;
                    if (cur_ratio<beta){
                        cur_ratio=beta;
                    }
                   
                    quantizer.set_eb(eb*cur_ratio);
                }

               
               
                
                
                    
                uint8_t cur_interpolator=interpolator_id;
                uint8_t cur_direction=direction_sequence_id;
                
                //if(!blockwiseTuning){
                    if (levelwise_predictor_levels==0){
                        cur_interpolator=interpolator_id;
                        cur_direction=direction_sequence_id;
                    }
                    else{
                        if (level-1<levelwise_predictor_levels){
                            cur_interpolator=interpAlgo_list[level-1];
                            cur_direction=interpDirection_list[level-1];
                        }
                        else{
                            cur_interpolator=interpAlgo_list[levelwise_predictor_levels-1];
                            cur_direction=interpDirection_list[levelwise_predictor_levels-1];
                        }
                    }
               // }
                
                
                
                size_t stride = 1U << (level - 1);
                size_t cur_blocksize;
                
                //if (blockwiseTuning){
               //     cur_blocksize=blocksize;
               // }
               // else 
                    if (fixBlockSize>0){
                    cur_blocksize=fixBlockSize;
                }
                else{
                    cur_blocksize=blocksize*stride;
                }
                
                //std::cout<<cur_blocksize<<std::endl;

                auto inter_block_range = std::make_shared<
                        QoZ::multi_dimensional_range<T, N>>(decData,
                                                           std::begin(global_dimensions), std::end(global_dimensions),
                                                           cur_blocksize, 0);
                auto inter_begin = inter_block_range->begin();
                auto inter_end = inter_block_range->end();
                
                //timer.stop("prep");
                

                //if (!blockwiseTuning){
                    for (auto block = inter_begin; block != inter_end; ++block) {

                        auto start_idx=block.get_global_index();
                        auto end_idx = start_idx;
                        for (int i = 0; i < N; i++) {
                            end_idx[i] += cur_blocksize;
                            if (end_idx[i] > global_dimensions[i] - 1) {
                                end_idx[i] = global_dimensions[i] - 1;
                            }
                        }
                        /*
                        if (blockwiseTuning){
                            
        
                            block_interpolation(decData, start_idx, end_idx, PB_recover,
                                            interpolators[interpAlgo_list[op_index]], interpDirection_list[op_index], stride);
                            op_index++;
                        }
                       */
                       //else{
                            block_interpolation(decData, block.get_global_index(), end_idx, PB_recover,
                                            interpolators[cur_interpolator], cur_direction, stride,0,cross_block);
                       // }
                       

                    }
               // }
                /*
                else{
                    for (auto block = inter_begin; block != inter_end; ++block) {

                        auto start_idx=block.get_global_index();
                        auto end_idx = start_idx;
                        for (int i = 0; i < N; i++) {
                            end_idx[i] += cur_blocksize;
                            if (end_idx[i] > global_dimensions[i] - 1) {
                                end_idx[i] = global_dimensions[i] - 1;
                            }
                        }
                        
                     
                            
        
                        block_interpolation(decData, start_idx, end_idx, PB_recover,
                                            interpolators[interpAlgo_list[op_index]], interpDirection_list[op_index], stride);
                        op_index++;
                     
                       
                      

                    }


                }
                */
                
                
               
            }
            quantizer.postdecompress_data();
           
            

            return decData;
        }

        T *decompress_block(uchar const *cmpData, const size_t &cmpSize, T *decData) {

            size_t remaining_length = cmpSize;
            uchar *buffer = lossless.decompress(cmpData, remaining_length);
            int levelwise_predictor_levels;
            int blockwiseTuning;
            uchar const *buffer_pos = buffer;
            std::vector <uint8_t> interpAlgo_list;
            std::vector <uint8_t> interpDirection_list;
            int fixBlockSize;
            //size_t maxStep;
           
            read(global_dimensions.data(), N, buffer_pos, remaining_length);
           
            read(blocksize, buffer_pos, remaining_length);
            
            read(interpolator_id, buffer_pos, remaining_length);
            
            read(direction_sequence_id, buffer_pos, remaining_length);
           
            read(alpha,buffer_pos,remaining_length);
            
            read(beta,buffer_pos,remaining_length);
           
            read(maxStep,buffer_pos,remaining_length);
           
            read(levelwise_predictor_levels,buffer_pos);
            
            read(blockwiseTuning,buffer_pos);

            read(fixBlockSize,buffer_pos);
            size_t cross_block=0;
            read(cross_block,buffer_pos);
           // std::cout<<"step 1 "<<std::endl;
            if(blockwiseTuning){
                size_t ops_num;
                read(ops_num,buffer_pos);
                interpAlgo_list=std::vector <uint8_t>(ops_num,0);
                interpDirection_list=std::vector <uint8_t>(ops_num,0);
                read(interpAlgo_list.data(),ops_num,buffer_pos);
                read(interpDirection_list.data(),ops_num,buffer_pos);
                



            }
            
            else if(levelwise_predictor_levels>0){
                interpAlgo_list=std::vector <uint8_t>(levelwise_predictor_levels,0);
                interpDirection_list=std::vector <uint8_t>(levelwise_predictor_levels,0);
                read(interpAlgo_list.data(),levelwise_predictor_levels,buffer_pos);
                read(interpDirection_list.data(),levelwise_predictor_levels,buffer_pos);
            }

            //std::cout<<cross_block<<std::endl;
            //std::cout<<"step 2"<<std::endl;

            init();
           // std::cout<<"step 3 "<<std::endl;
            //QoZ::Timer timer(true);
            quantizer.load(buffer_pos, remaining_length);
            encoder.load(buffer_pos, remaining_length);
            quant_inds = encoder.decode(buffer_pos, num_elements);

            encoder.postprocess_decode();

            lossless.postdecompress_data(buffer);
           // std::cout<<"step 4 "<<std::endl;
            //timer.stop("decode");
            //timer.start();
            double eb = quantizer.get_eb();
            if(!anchor){
                *decData = quantizer.recover(0, quant_inds[quant_index++]);
            }
            
            else{

             
                 recover_grid(decData,global_dimensions,maxStep);
                    
                interpolation_level--;

                
            }
            size_t op_index=0;

            //std::cout<<"step 5 "<<std::endl;


    
            for (uint level = interpolation_level; level > 0 && level <= interpolation_level; level--) {
                if (alpha<0) {
                    if (level >= 3) {
                        quantizer.set_eb(eb * eb_ratio);
                    } else {
                        quantizer.set_eb(eb);
                    }
                }
                
                else if (alpha>=1){
                    
                    
                    double cur_ratio=pow(alpha,level-1);
                    if (cur_ratio>beta){
                        cur_ratio=beta;
                    }
                    
                    quantizer.set_eb(eb/cur_ratio);
                }
                else{
                    
                    
                    double cur_ratio=1-(level-1)*alpha;
                    if (cur_ratio<beta){
                        cur_ratio=beta;
                    }
                   
                    quantizer.set_eb(eb*cur_ratio);
                }
                //std::cout<<"step 6 "<<std::endl;
               
               
                
                
                    
                uint8_t cur_interpolator=interpolator_id;
                uint8_t cur_direction=direction_sequence_id;
                
                
                
                
                
                size_t stride = 1U << (level - 1);
                size_t cur_blocksize=blocksize;
                
              

                auto inter_block_range = std::make_shared<
                        QoZ::multi_dimensional_range<T, N>>(decData,
                                                           std::begin(global_dimensions), std::end(global_dimensions),
                                                           cur_blocksize, 0);
                auto inter_begin = inter_block_range->begin();
                auto inter_end = inter_block_range->end();
                
                //timer.stop("prep");
                

                
              
                    for (auto block = inter_begin; block != inter_end; ++block) {

                        auto start_idx=block.get_global_index();
                        auto end_idx = start_idx;
                        for (int i = 0; i < N; i++) {
                            end_idx[i] += cur_blocksize;
                            if (end_idx[i] > global_dimensions[i] - 1) {
                                end_idx[i] = global_dimensions[i] - 1;
                            }
                        }
                        
                     
                            
        
                        block_interpolation(decData, start_idx, end_idx, PB_recover,
                                            interpolators[interpAlgo_list[op_index]], interpDirection_list[op_index], stride,0,cross_block);
                        op_index++;
                     
                       
                      

                    }


                
                
                
               
            }
            quantizer.postdecompress_data();
           
            

            return decData;
        }
        // compress given the error bound
        uchar *compress( Config &conf, T *data, size_t &compressed_size,int tuning=0,int start_level=0,int end_level=0) {
            
            //tuning 0: normal compress 1:tuning to return qbins and psnr 2: tuning to return prediction loss
            Timer timer;
            timer.start();
            std::copy_n(conf.dims.begin(), N, global_dimensions.begin());
            blocksize = conf.interpBlockSize;
            
            maxStep=conf.maxStep;
            
            interpolator_id = conf.interpAlgo;
            direction_sequence_id = conf.interpDirection;
            alpha=conf.alpha;
            beta=conf.beta;





            std::vector<uint8_t>interp_ops;
            std::vector<uint8_t>interp_dirs;
            size_t cross_block=conf.crossBlock;
            init();
            if (tuning){
                std::vector<int>().swap(quant_inds);
                std::vector<int>().swap(conf.quant_bins);
                conf.quant_bin_counts=std::vector<size_t>(interpolation_level,0);
                //conf.quant_bin_counts.reserve(interpolation_level);
                //conf.pred_square_error=0.0;
                conf.decomp_square_error=0.0;

            }
            if(tuning==0 and conf.peTracking){
                prediction_errors.resize(num_elements,0);
                peTracking=1;
            }
            quant_inds.reserve(num_elements);
            size_t interp_compressed_size = 0;

            double eb = quantizer.get_eb();

//            printf("Absolute error bound = %.5f\n", eb);

            if (start_level<=0 or start_level>interpolation_level ){

                start_level=interpolation_level;

                
            } 
            if(end_level>=start_level or end_level<0){
                end_level=0;
            }


            if(!anchor){
                quant_inds.push_back(quantizer.quantize_and_overwrite(*data, 0));
            }
            else if (start_level==interpolation_level){
                if(tuning){
                    conf.quant_bin_counts[start_level-1]=quant_inds.size();
                }
                    
                build_grid(conf,data,maxStep,tuning);
                start_level--;
            }

            
            double predict_error=0.0;
          

            
            int levelwise_predictor_levels=conf.interpAlgo_list.size();

            
            

            for (uint level = start_level; level > end_level && level <= start_level; level--) {

                

                if (alpha<0) {
                    if (level >= 3) {
                        quantizer.set_eb(eb * eb_ratio);
                    } else {
                        quantizer.set_eb(eb);
                    }
                }
                else if (alpha>=1){
                    
                    
                    double cur_ratio=pow(alpha,level-1);
                    if (cur_ratio>beta){
                        cur_ratio=beta;
                    }
                    
                    quantizer.set_eb(eb/cur_ratio);
                }
                else{
                    
                    
                    double cur_ratio=1-(level-1)*alpha;
                    if (cur_ratio<beta){
                        cur_ratio=beta;
                    }
                    
                    quantizer.set_eb(eb*cur_ratio);
                }
              
               
                    
                    
                   



              

                int cur_interpolator;
                int cur_direction;
                //if(!conf.blockwiseTuning){
                    if (levelwise_predictor_levels==0){
                        cur_interpolator=interpolator_id;
                        cur_direction=direction_sequence_id;
                    }
                    else{
                        if (level-1<levelwise_predictor_levels){
                            cur_interpolator=conf.interpAlgo_list[level-1];
                            cur_direction=conf.interpDirection_list[level-1];
                        }
                        else{
                            cur_interpolator=conf.interpAlgo_list[levelwise_predictor_levels-1];
                            cur_direction=conf.interpDirection_list[levelwise_predictor_levels-1];
                        }
                    }
                //}
                
                uint stride = 1U << (level - 1);
                size_t cur_blocksize;
                //if (conf.blockwiseTuning){
                    //cur_blocksize=blocksize;
                //}
                //else 
                if (conf.fixBlockSize>0){
                    cur_blocksize=conf.fixBlockSize;
                }
                else{
                    cur_blocksize=blocksize*stride;
                }
               
                
                auto inter_block_range = std::make_shared<
                        QoZ::multi_dimensional_range<T, N>>(data, std::begin(global_dimensions),
                                                           std::end(global_dimensions),
                                                           cur_blocksize, 0);

                auto inter_begin = inter_block_range->begin();
                auto inter_end = inter_block_range->end();

                

                //if(!conf.blockwiseTuning){
               
                    for (auto block = inter_begin; block != inter_end; ++block) {




                        


                        auto start_idx=block.get_global_index();
                        auto end_idx = start_idx;
                        for (int i = 0; i < N; i++) {
                            end_idx[i] += cur_blocksize ;
                            if (end_idx[i] > global_dimensions[i] - 1) {
                                end_idx[i] = global_dimensions[i] - 1;
                            }
                        }
                        /*
                        if (conf.blockwiseTuning){
                            size_t cur_element_num=1;
                            for (int i=0;i<N;i++){
                                cur_element_num*=(end_idx[i]-start_idx[i]+1);
                            }
                            
                            std::vector<T> orig_block(cur_element_num,0);
                            size_t local_idx=0;
                            if(N==2){
                                for(size_t x=start_idx[0];x<=end_idx[0];x++){
                                    for(size_t y=start_idx[1];y<=end_idx[1];y++){
                                        size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1];
                                        orig_block[local_idx]=data[global_idx];
                                        local_idx++;
                                    }
                                }
                            }

                            else if(N==3){
                                for(size_t x=start_idx[0];x<=end_idx[0];x++){
                                    for(size_t y=start_idx[1];y<=end_idx[1];y++){
                                        for(size_t z=start_idx[2];z<=end_idx[2];z++){
                                            size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1]+z*dimension_offsets[2];
                                            orig_block[local_idx]=data[global_idx];
                                            local_idx++;
                                        }
                                    }
                                }
                            }
                            
                            uint8_t best_op=QoZ::INTERP_ALGO_CUBIC;
                            uint8_t best_dir=0;
                            double best_loss=9e10;
                            std::vector<int> op_candidates={QoZ::INTERP_ALGO_LINEAR,QoZ::INTERP_ALGO_CUBIC};
                            std::vector<int> dir_candidates={0,QoZ::factorial(N) - 1};
                            for (auto &interp_op:op_candidates) {
                                for (auto &interp_direction: dir_candidates) {
                                    double cur_loss=block_interpolation(data, start_idx, end_idx, PB_predict_overwrite,
                                            interpolators[interp_op], interp_direction, stride,2);

                                    if(cur_loss<best_loss){
                                        best_loss=cur_loss;
                                        best_op=interp_op;
                                        best_dir=interp_direction;
                                    }

                                    size_t local_idx=0;
                                    if(N==2){
                                        for(size_t x=start_idx[0];x<=end_idx[0];x++){
                                            for(size_t y=start_idx[1];y<=end_idx[1];y++){
                                                size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1];
                                                data[global_idx]=orig_block[local_idx];
                                                local_idx++;
                                            }
                                        }
                                    }
                                    else if(N==3){
                                        for(size_t x=start_idx[0];x<=end_idx[0];x++){
                                            for(size_t y=start_idx[1];y<=end_idx[1];y++){
                                                for(size_t z=start_idx[2];z<=end_idx[2];z++){
                                                    size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1]+z*dimension_offsets[2];
                                                    data[global_idx]=orig_block[local_idx];
                                                    local_idx++;
                                                }
                                            }
                                        }
                                    }

                                }
                            }

                            interp_ops.push_back(best_op);
                            interp_dirs.push_back(best_dir);
                            block_interpolation(data, start_idx, end_idx, PB_predict_overwrite,
                                            interpolators[best_op], best_dir, stride,0);


                            
                        }

                        

                        */

                        

                        //else{
                           if(peTracking)
                        
                                predict_error+=block_interpolation(data, start_idx, end_idx, PB_predict_overwrite,
                                                interpolators[cur_interpolator], cur_direction, stride,3,cross_block);
                            else
                                predict_error+=block_interpolation(data, start_idx, end_idx, PB_predict_overwrite,
                                                interpolators[cur_interpolator], cur_direction, stride,tuning,cross_block);

                        //}
                    
                        
                    }
                //}
                /*
                else{
                    for (auto block = inter_begin; block != inter_end; ++block) {


                        auto start_idx=block.get_global_index();
                        auto end_idx = start_idx;
                        for (int i = 0; i < N; i++) {
                            end_idx[i] += cur_blocksize ;
                            if (end_idx[i] > global_dimensions[i] - 1) {
                                end_idx[i] = global_dimensions[i] - 1;
                            }
                        }


                        size_t cur_element_num=1;
                        for (int i=0;i<N;i++){
                            cur_element_num*=(end_idx[i]-start_idx[i]+1);
                        }
                            
                        std::vector<T> orig_block(cur_element_num,0);
                        size_t local_idx=0;
                        if(N==2){
                            for(size_t x=start_idx[0];x<=end_idx[0];x++){
                                for(size_t y=start_idx[1];y<=end_idx[1];y++){
                                    size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1];
                                    orig_block[local_idx]=data[global_idx];
                                    local_idx++;
                                }
                            }
                        }

                        else if(N==3){
                            for(size_t x=start_idx[0];x<=end_idx[0];x++){
                                for(size_t y=start_idx[1];y<=end_idx[1];y++){
                                    for(size_t z=start_idx[2];z<=end_idx[2];z++){
                                        size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1]+z*dimension_offsets[2];
                                        orig_block[local_idx]=data[global_idx];
                                        local_idx++;
                                    }
                                }
                            }
                        }
                            
                        uint8_t best_op=QoZ::INTERP_ALGO_CUBIC;
                        uint8_t best_dir=0;
                        double best_loss=9e10;
                        std::vector<int> op_candidates={QoZ::INTERP_ALGO_LINEAR,QoZ::INTERP_ALGO_CUBIC};
                        std::vector<int> dir_candidates={0,QoZ::factorial(N) - 1};
                        for (auto &interp_op:op_candidates) {
                            for (auto &interp_direction: dir_candidates) {
                                double cur_loss=block_interpolation(data, start_idx, end_idx, PB_predict_overwrite,
                                    interpolators[interp_op], interp_direction, stride,2);

                                if(cur_loss<best_loss){
                                    best_loss=cur_loss;
                                    best_op=interp_op;
                                    best_dir=interp_direction;
                                }

                                size_t local_idx=0;
                                if(N==2){
                                    for(size_t x=start_idx[0];x<=end_idx[0];x++){
                                        for(size_t y=start_idx[1];y<=end_idx[1];y++){
                                            size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1];
                                            data[global_idx]=orig_block[local_idx];
                                            local_idx++;
                                        }
                                    }
                                }
                                else if(N==3){
                                    for(size_t x=start_idx[0];x<=end_idx[0];x++){
                                        for(size_t y=start_idx[1];y<=end_idx[1];y++){
                                            for(size_t z=start_idx[2];z<=end_idx[2];z++){
                                                size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1]+z*dimension_offsets[2];
                                                data[global_idx]=orig_block[local_idx];
                                                local_idx++;
                                            }
                                        }
                                    }
                                }

                            }
                        }

                        interp_ops.push_back(best_op);
                        interp_dirs.push_back(best_dir);
                        block_interpolation(data, start_idx, end_idx, PB_predict_overwrite,
                                        interpolators[best_op], best_dir, stride,0);



                    }

                }
                */




                if(tuning){
                        
                        conf.quant_bin_counts[level-1]=quant_inds.size();
                }
                

            }
            
//            std::cout << "Number of data point = " << num_elements << std::endl;
//            std::cout << "quantization element = " << quant_inds.size() << std::endl;
             
             
            //timer.start();

//            writefile("pred.dat", preds.data(), num_elements);
//            writefile("quant.dat", quant_inds.data(), num_elements);
            quantizer.set_eb(eb);
            ///std::cout<<"000"<<std::endl;
            if(peTracking){
                conf.predictionErrors=prediction_errors;
            }
            if (tuning){
               
                
                conf.quant_bins=quant_inds;
                std::vector<int>().swap(quant_inds);

                //quantizer.clear();
                conf.decomp_square_error=predict_error;
                size_t bufferSize = 1;
                uchar *buffer = new uchar[bufferSize];
                buffer[0]=0;
                
                return buffer;
            }
            //std::cout<<"predict_ended"<<std::endl;
            if(conf.verbose)
                timer.stop("prediction");
            //timer.start();
            //assert(quant_inds.size() == num_elements);
             //std::cout<<"1"<<std::endl;
            size_t bufferSize = 1.5 * (quant_inds.size() * sizeof(T) + quantizer.size_est());
            uchar *buffer = new uchar[bufferSize];
            uchar *buffer_pos = buffer;
            //std::cout<<"2"<<std::endl;
            write(global_dimensions.data(), N, buffer_pos);
            write(blocksize, buffer_pos);
            
            //std::cout<<"3"<<std::endl;
            write(interpolator_id, buffer_pos);
            write(direction_sequence_id, buffer_pos);
            write(alpha,buffer_pos);
            write(beta,buffer_pos);
            //std::cout<<"4"<<std::endl;
            write(maxStep,buffer_pos);
            write(levelwise_predictor_levels,buffer_pos);
            write(conf.blockwiseTuning,buffer_pos);
            write(conf.fixBlockSize,buffer_pos);
            write(cross_block,buffer_pos);
            //std::cout<<"5"<<std::endl;
            if(conf.blockwiseTuning){
                size_t ops_num=interp_ops.size();
                write(ops_num,buffer_pos);
                write(interp_ops.data(),ops_num,buffer_pos);
                write(interp_dirs.data(),ops_num,buffer_pos);

            }
            else if(levelwise_predictor_levels>0){
                write(conf.interpAlgo_list.data(),levelwise_predictor_levels,buffer_pos);
                write(conf.interpDirection_list.data(),levelwise_predictor_levels,buffer_pos);
            }
           //std::cout<<"6"<<std::endl;
            quantizer.save(buffer_pos);
            quantizer.postcompress_data();
            quantizer.clear();
            //std::cout<<"7"<<std::endl;
          

           
            encoder.preprocess_encode(quant_inds, 0);
           // std::cout<<"7.1"<<std::endl;
            encoder.save(buffer_pos);
           // std::cout<<"7.2"<<std::endl;
            encoder.encode(quant_inds, buffer_pos);
          //  std::cout<<"7.3"<<std::endl;
            encoder.postprocess_encode();
           // std::cout<<"8"<<std::endl;
            
            //timer.stop("Coding");
            //timer.start();
            assert(buffer_pos - buffer < bufferSize);

            
            uchar *lossless_data = lossless.compress(buffer,
                                                     buffer_pos - buffer,
                                                     compressed_size);
            lossless.postcompress_data(buffer);
            
            //timer.stop("Lossless") ;
            

            compressed_size += interp_compressed_size;
            
            //quantizer.print_unpred();
            return lossless_data;
        }


        // compress given the error bound
        uchar *compress_block( Config &conf, T *data, size_t &compressed_size,int tuning=0,int start_level=0,int end_level=0) {
            
            //tuning 0: normal compress 1:tuning to return qbins and psnr 2: tuning to return prediction loss
            Timer timer;
            timer.start();
            std::copy_n(conf.dims.begin(), N, global_dimensions.begin());
            blocksize = conf.interpBlockSize;
            
            maxStep=conf.maxStep;
            
            interpolator_id = conf.interpAlgo;
            direction_sequence_id = conf.interpDirection;
            alpha=conf.alpha;
            beta=conf.beta;
            //mark=std::vector<bool>(conf.num,false);
            size_t cross_block=conf.crossBlock;



            std::vector<uint8_t>interp_ops;
            std::vector<uint8_t>interp_dirs;
            init();
            if (tuning){
                std::vector<int>().swap(quant_inds);
                std::vector<int>().swap(conf.quant_bins);
                conf.quant_bin_counts=std::vector<size_t>(interpolation_level,0);
                //conf.quant_bin_counts.reserve(interpolation_level);
                //conf.pred_square_error=0.0;
                conf.decomp_square_error=0.0;

            }
            if(tuning==0 and conf.peTracking){
                prediction_errors.resize(num_elements,0);
                peTracking=1;
            }
            quant_inds.reserve(num_elements);
            size_t interp_compressed_size = 0;

            double eb = quantizer.get_eb();

//            printf("Absolute error bound = %.5f\n", eb);

            if (start_level<=0 or start_level>interpolation_level ){
                
                start_level=interpolation_level;

                
            } 
            if(end_level>=start_level or end_level<0){
                end_level=0;
            }


            if(!anchor){
                quant_inds.push_back(quantizer.quantize_and_overwrite(*data, 0));
                //if(tuning==0)
                    //mark[0]=true;
            }
            else if (start_level==interpolation_level){
                if(tuning){
                    conf.quant_bin_counts[start_level-1]=quant_inds.size();
                }
                    
                build_grid(conf,data,maxStep,tuning);
                start_level--;
            }

            
            double predict_error=0.0;
          

            
            int levelwise_predictor_levels=conf.interpAlgo_list.size();

            
            

            for (uint level = start_level; level > end_level && level <= start_level; level--) {

                

                if (alpha<0) {
                    if (level >= 3) {
                        quantizer.set_eb(eb * eb_ratio);
                    } else {
                        quantizer.set_eb(eb);
                    }
                }
                else if (alpha>=1){
                    
                    
                    double cur_ratio=pow(alpha,level-1);
                    if (cur_ratio>beta){
                        cur_ratio=beta;
                    }
                    
                    quantizer.set_eb(eb/cur_ratio);
                }
                else{
                    
                    
                    double cur_ratio=1-(level-1)*alpha;
                    if (cur_ratio<beta){
                        cur_ratio=beta;
                    }
                    
                    quantizer.set_eb(eb*cur_ratio);
                }
              
               
                    
                    
                   



              

                int cur_interpolator;
                int cur_direction;
                
                
                uint stride = 1U << (level - 1);
                size_t cur_blocksize=blocksize;
               
               
                
                auto inter_block_range = std::make_shared<
                        QoZ::multi_dimensional_range<T, N>>(data, std::begin(global_dimensions),
                                                           std::end(global_dimensions),
                                                           cur_blocksize, 0);

                auto inter_begin = inter_block_range->begin();
                auto inter_end = inter_block_range->end();

                size_t blockwiseSampleBlockSize=(level<=2)?conf.blockwiseSampleBlockSize:cur_blocksize;
                if(blockwiseSampleBlockSize>cur_blocksize)
                    blockwiseSampleBlockSize=cur_blocksize;
                //size_t blockwiseSampleBlockSize=cur_blocksize;

                
                for (auto block = inter_begin; block != inter_end; ++block) {


                    auto start_idx=block.get_global_index();
                    auto end_idx = start_idx;
                    auto sample_end_idx=start_idx;
                    for (int i = 0; i < N; i++) {
                        //std::cout<<start_idx[i]<<std::endl;
                        end_idx[i] += cur_blocksize ;
                        if (end_idx[i] > global_dimensions[i] - 1) {
                            end_idx[i] = global_dimensions[i] - 1;
                        }
                        sample_end_idx[i]+=blockwiseSampleBlockSize;
                        if (sample_end_idx[i] > global_dimensions[i] - 1) {
                            sample_end_idx[i] = global_dimensions[i] - 1;
                        }
                        //addtest
                    }


                    //size_t cur_element_num=1;
                    //for (int i=0;i<N;i++){
                    //    cur_element_num*=(end_idx[i]-start_idx[i]+1);
                    //}
                    size_t sampled_element_num=1;
                    for(int i=0;i<N;i++){

                        sampled_element_num*=(sample_end_idx[i]-start_idx[i]+1);

                    }

                    std::vector<T> orig_sampled_block(sampled_element_num,0);
                    size_t local_idx=0;
                    if(N==2){
                        for(size_t x=start_idx[0];x<=sample_end_idx[0] ;x++){
                            for(size_t y=start_idx[1];y<=sample_end_idx[1];y++){
                                size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1];
                                orig_sampled_block[local_idx]=data[global_idx];
                                local_idx++;
                            }
                        }
                    }

                    else if(N==3){
                        for(size_t x=start_idx[0];x<=sample_end_idx[0] ;x++){
                            for(size_t y=start_idx[1];y<=sample_end_idx[1] ;y++){
                                for(size_t z=start_idx[2];z<=sample_end_idx[2];z++){
                                    size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1]+z*dimension_offsets[2];
                                    orig_sampled_block[local_idx]=data[global_idx];
                                    local_idx++;
                                }
                            }
                        }
                    }
                            
                    uint8_t best_op=QoZ::INTERP_ALGO_CUBIC;
                    uint8_t best_dir=0;
                    double best_loss=std::numeric_limits<double>::max();
                    std::vector<int> op_candidates={QoZ::INTERP_ALGO_LINEAR,QoZ::INTERP_ALGO_CUBIC};
                    std::vector<int> dir_candidates={0,QoZ::factorial(N) - 1};
                    for (auto &interp_op:op_candidates) {
                        for (auto &interp_direction: dir_candidates) {
                            double cur_loss=block_interpolation(data, start_idx, sample_end_idx, PB_predict_overwrite,
                                interpolators[interp_op], interp_direction, stride,2);

                            if(cur_loss<best_loss){
                                best_loss=cur_loss;
                                best_op=interp_op;
                                best_dir=interp_direction;
                            }

                            size_t local_idx=0;
                            if(N==2){
                                for(size_t x=start_idx[0];x<=sample_end_idx[0];x++){
                                    for(size_t y=start_idx[1];y<=sample_end_idx[1];y++){
                                        size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1];
                                        data[global_idx]=orig_sampled_block[local_idx];
                                        local_idx++;
                                    }
                                }
                            }
                            else if(N==3){
                                for(size_t x=start_idx[0];x<=sample_end_idx[0];x++){
                                    for(size_t y=start_idx[1];y<=sample_end_idx[1];y++){
                                        for(size_t z=start_idx[2];z<=sample_end_idx[2];z++){
                                            size_t global_idx=x*dimension_offsets[0]+y*dimension_offsets[1]+z*dimension_offsets[2];
                                            data[global_idx]=orig_sampled_block[local_idx];
                                            local_idx++;
                                        }
                                    }
                                }
                            }

                        }
                    }

                    interp_ops.push_back(best_op);
                    interp_dirs.push_back(best_dir);
                    if (peTracking)
                        block_interpolation(data, start_idx, end_idx, PB_predict_overwrite,
                                    interpolators[best_op], best_dir, stride,3,cross_block);
                    else
                        block_interpolation(data, start_idx, end_idx, PB_predict_overwrite,
                                    interpolators[best_op], best_dir, stride,0,cross_block);



                }

            



                if(tuning){
                        
                    conf.quant_bin_counts[level-1]=quant_inds.size();
                }
                

            }
            
//            std::cout << "Number of data point = " << num_elements << std::endl;
//            std::cout << "quantization element = " << quant_inds.size() << std::endl;
             
             
            //timer.start();

//            writefile("pred.dat", preds.data(), num_elements);
//            writefile("quant.dat", quant_inds.data(), num_elements);
            quantizer.set_eb(eb);
            if(peTracking){
                std::cout<<quant_inds.size()<<std::endl;
                std::cout<<prediction_errors.size()<<std::endl;
                conf.predictionErrors=prediction_errors;
            }
            if (tuning){
               
                
                conf.quant_bins=quant_inds;
                std::vector<int>().swap(quant_inds);

                //quantizer.clear();
                conf.decomp_square_error=predict_error;
                size_t bufferSize = 1;
                uchar *buffer = new uchar[bufferSize];
                buffer[0]=0;
                
                return buffer;
            }
            //std::cout<<"predict_ended"<<std::endl;
            if(conf.verbose)
                timer.stop("prediction");
           
            //assert(quant_inds.size() == num_elements);

            size_t bufferSize = 1.5 * (quant_inds.size() * sizeof(T) + quantizer.size_est());
            uchar *buffer = new uchar[bufferSize];
            uchar *buffer_pos = buffer;

            write(global_dimensions.data(), N, buffer_pos);
            write(blocksize, buffer_pos);
            

            write(interpolator_id, buffer_pos);
            write(direction_sequence_id, buffer_pos);
            write(alpha,buffer_pos);
            write(beta,buffer_pos);
           
            write(maxStep,buffer_pos);
            write(levelwise_predictor_levels,buffer_pos);
            write(conf.blockwiseTuning,buffer_pos);
            write(conf.fixBlockSize,buffer_pos);
            write(cross_block,buffer_pos);
            if(conf.blockwiseTuning){
                size_t ops_num=interp_ops.size();
                write(ops_num,buffer_pos);
                write(interp_ops.data(),ops_num,buffer_pos);
                write(interp_dirs.data(),ops_num,buffer_pos);
                

            }
            else if(levelwise_predictor_levels>0){
                write(conf.interpAlgo_list.data(),levelwise_predictor_levels,buffer_pos);
                write(conf.interpDirection_list.data(),levelwise_predictor_levels,buffer_pos);
            }
           
            quantizer.save(buffer_pos);
            quantizer.postcompress_data();
            quantizer.clear();
          

           
            encoder.preprocess_encode(quant_inds, 0);
            encoder.save(buffer_pos);
            encoder.encode(quant_inds, buffer_pos);
            encoder.postprocess_encode();
            
            //timer.stop("Coding");
            //timer.start();
            assert(buffer_pos - buffer < bufferSize);

            
            uchar *lossless_data = lossless.compress(buffer,
                                                     buffer_pos - buffer,
                                                     compressed_size);
            lossless.postcompress_data(buffer);
            
            //timer.stop("Lossless") ;
            

            compressed_size += interp_compressed_size;
            
            //quantizer.print_unpred();
            return lossless_data;
        }


        uchar *encoding_lossless(Config &conf,std::vector<int> &q_inds,size_t &compressed_size,bool write_metadata=false) {



            size_t bufferSize = 1.5 * (q_inds.size() * sizeof(T) + quantizer.size_est());//original is 3
            uchar *buffer = new uchar[bufferSize];
            uchar *buffer_pos = buffer;
           
            if(write_metadata){
                write(conf.dims.data(), N, buffer_pos);
                write(conf.interpBlockSize, buffer_pos);
                write(conf.interpAlgo, buffer_pos);
                write(conf.interpDirection, buffer_pos);
                write(conf.alpha,buffer_pos);
                write(conf.beta,buffer_pos);
               
                write(conf.maxStep,buffer_pos);
                write(conf.levelwisePredictionSelection,buffer_pos);
                write(conf.fixBlockSize,buffer_pos);
                if(conf.levelwisePredictionSelection>0){
                    write(conf.interpAlgo_list.data(),conf.levelwisePredictionSelection,buffer_pos);
                    write(conf.interpDirection_list.data(),conf.levelwisePredictionSelection,buffer_pos);
                }
            }



            quantizer.save(buffer_pos);
           
            quantizer.clear();
            
            quantizer.postcompress_data();
           

            //timer.start();
            encoder.preprocess_encode(q_inds, 0);
            encoder.save(buffer_pos);
            encoder.encode(q_inds, buffer_pos);
            encoder.postprocess_encode();
           

//            timer.stop("Coding");
            assert(buffer_pos - buffer < bufferSize);

            //timer.start();
            uchar *lossless_data = lossless.compress(buffer,
                                                     buffer_pos - buffer,
                                                     compressed_size);
            lossless.postcompress_data(buffer);
//            timer.stop("Lossless");

           // compressed_size += interp_compressed_size;
            return lossless_data;

        }

        void set_eb(double eb){
            quantizer.set_eb(eb);
        }

    private:

        enum PredictorBehavior {
            PB_predict_overwrite, PB_predict, PB_recover
        };
        
        void init() {
            assert(blocksize % 2 == 0 && "Interpolation block size should be even numbers");
            num_elements = 1;

            interpolation_level = -1;

            for (int i = 0; i < N; i++) {
                if (interpolation_level < ceil(log2(global_dimensions[i]))) {
                    interpolation_level = (uint) ceil(log2(global_dimensions[i]));
                }
                num_elements *= global_dimensions[i];
            }

            if (maxStep>0){
               
                int max_interpolation_level=(uint)log2(maxStep)+1;
                if (max_interpolation_level<=interpolation_level){
                    anchor=true;
                    interpolation_level=max_interpolation_level;
                }

            }


            dimension_offsets[N - 1] = 1;
            for (int i = N - 2; i >= 0; i--) {
                dimension_offsets[i] = dimension_offsets[i + 1] * global_dimensions[i + 1];
            }

            dimension_sequences = std::vector<std::array<int, N>>();
            auto sequence = std::array<int, N>();
            for (int i = 0; i < N; i++) {
                sequence[i] = i;
            }
            do {
                dimension_sequences.push_back(sequence);
            } while (std::next_permutation(sequence.begin(), sequence.end()));

            
        }
       
        void build_grid(Config &conf, T *data,size_t maxStep,int tuning=0){
            
            assert(maxStep>0);

           
            if(tuning>1)
                return;
            /*
            else if(tuning==1 and conf.sampleBlockSize<conf.maxStep and conf.tuningTarget==QoZ::TUNING_TARGET_RD){
                //std::cout<<"dd"<<std::endl;
                quantizer.insert_unpred(*data);
                return;

            }
            */
            if (N==2){
                for (size_t x=maxStep*(tuning==1);x<conf.dims[0];x+=maxStep){
                    for (size_t y=maxStep*(tuning==1);y<conf.dims[1];y+=maxStep){

                        quantizer.insert_unpred(*(data+x*conf.dims[1]+y));
                        //quant_inds.push_back(0);
                    }
                }
            }
            else if(N==3){
                for (size_t x=maxStep*(tuning==1);x<conf.dims[0];x+=maxStep){
                    for (size_t y=maxStep*(tuning==1);y<conf.dims[1];y+=maxStep){
                        for(size_t z=maxStep*(tuning==1);z<conf.dims[2];z+=maxStep){
                            quantizer.insert_unpred(*(data+x*conf.dims[1]*conf.dims[2]+y*conf.dims[2]+z) );
                            //if(tuning==0)
                                //mark[x*conf.dims[1]*conf.dims[2]+y*conf.dims[2]+z]=true;



                            //quant_inds.push_back(0);

                        }

                        
                    }
                }

            }

        }

        
        void recover_grid(T *decData,const std::array<size_t,N>& global_dimensions,size_t maxStep){
            
            assert(maxStep>0);
            if (N==2){
                for (size_t x=0;x<global_dimensions[0];x+=maxStep){
                    for (size_t y=0;y<global_dimensions[1];y+=maxStep){

                        decData[x*global_dimensions[1]+y]=quantizer.recover_unpred();
                        //quant_index++;
                    }
                }
            }
            else if(N==3){
                for (size_t x=0;x<global_dimensions[0];x+=maxStep){
                    for (size_t y=0;y<global_dimensions[1];y+=maxStep){
                        for(size_t z=0;z<global_dimensions[2];z+=maxStep){
                            decData[x*global_dimensions[1]*global_dimensions[2]+y*global_dimensions[2]+z]=quantizer.recover_unpred();
                            //quant_index++;

                        }

                        
                    }
                }

            }

        }

        inline void quantize1(size_t idx, T &d, T pred) {
            if (idx >= 2 * 449 * 449 * 235 && idx < 3 * 449 * 449 * 235 &&
                fabs(d - pred) > max_error) {
                max_error = fabs(d - pred);
            }
            auto quant = quantizer.quantize_and_overwrite(d, pred);
//            quant_inds.push_back(quant);
            quant_inds[idx] = quant;
        }

        inline void quantize(size_t idx, T &d, T pred) {

//            preds[idx] = pred;
//            quant_inds[idx] = quantizer.quantize_and_overwrite(d, pred);
            //T orig=d;
            quant_inds.push_back(quantizer.quantize_and_overwrite(d, pred));
            //return fabs(d-orig);
        }

        inline double quantize_tuning(size_t idx, T &d, T pred, int mode=1) {

//            preds[idx] = pred;
//            quant_inds[idx] = quantizer.quantize_and_overwrite(d, pred);

            if (mode==1){
                T orig=d;
                quant_inds.push_back(quantizer.quantize_and_overwrite(d, pred));
                return (d-orig)*(d-orig);

            }
            else if (mode==2){
                double pred_error=fabs(d-pred);
                /*
                if(peTracking)
                    prediction_errors[idx]=pred_error;
                */
                int q_bin=quantizer.quantize_and_overwrite(d, pred,false);
                /*
                if(peTracking){
                    prediction_errors[idx]=pred_error;
                
                    quant_inds.push_back(q_bin);
                }
                */
                return pred_error;
            }
            else{
                double pred_error=fabs(d-pred);
               
                int q_bin=quantizer.quantize_and_overwrite(d, pred,false);
                
              
                prediction_errors[idx]=pred_error;
                
                quant_inds.push_back(q_bin);
                
                return pred_error;
            }
        }

        inline void recover(size_t idx, T &d, T pred) {
            d = quantizer.recover(pred, quant_inds[quant_index++]);
        };


        double block_interpolation_1d_cross(T *data, size_t begin, size_t end, size_t stride,
                                      const std::string &interp_func,
                                      const PredictorBehavior pb,int tuning=0,size_t cross_block=0,size_t axis_begin=0,size_t axis_stride=0,size_t cur_axis=0) {//cross block: 0: no cross 1: only front-cross 2: all cross
            size_t n = (end - begin) / stride + 1;
            if (n <= 1) {
                return 0;
            }
            double predict_error = 0;

            size_t stride3x = 3 * stride;
            size_t stride5x = 5 * stride;

            if (interp_func == "linear" || (n < 5 and cross_block==0 ) ) {//this place maybe some bug
                if (pb == PB_predict_overwrite) {

                    if (tuning){
                        for (size_t i = 1; i + 1 < n; i += 2) {
                            T *d = data + begin + i * stride;
                            predict_error+=quantize_tuning(d - data, *d, interp_linear(*(d - stride), *(d + stride)),tuning);
                        }
                        if (n % 2 == 0) {
                            size_t offset = begin + (n - 1) * stride;
                            T *d = data + offset;

                            if (cross_block==2 and axis_begin+n*axis_stride<global_dimensions[cur_axis]){
                                predict_error+=quantize_tuning(d - data, *d, interp_linear(*(d - stride), *(d + stride)),tuning);

                            }
                            else if (n >= 4 or (cross_block and  axis_begin >= (4-n)*axis_stride )) {
                                
                                predict_error+=quantize_tuning(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride)),tuning);
                              
                                  
                            } 
                            else { 
                                predict_error+=quantize_tuning(d - data, *d, *(d - stride),tuning);
                            }
                        }

                    }
                    else{
                       

                        for (size_t i = 1; i + 1 < n; i += 2) {
                            T *d = data + begin + i * stride;

                            quantize(d - data, *d, interp_linear(*(d - stride), *(d + stride)));

                            //mark[begin + i * stride]=true;
                        }
                      

                        if (n % 2 == 0) {
                            size_t offset = begin + (n - 1) * stride;
                            T *d = data + offset;

                            if (cross_block==2 and axis_begin+n*axis_stride<global_dimensions[cur_axis]){
                                quantize(d - data, *d, interp_linear(*(d - stride), *(d + stride)));

                            }
                            else if (n >= 4 or (cross_block and axis_begin >= (4-n)*axis_stride )) {
                                
                               quantize(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride) ) );
                              
                                  
                            } 
                            else { 
                                quantize(d - data, *d, *(d - stride) );
                            }
                            //mark[offset]=true;
                        }
                       

                    }

                } else {
                    for (size_t i = 1; i + 1 < n; i += 2) {
                        T *d = data + begin + i * stride;
                        recover(d - data, *d, interp_linear(*(d - stride), *(d + stride)));
                    }
                    if (n % 2 == 0) {
                        size_t offset = begin + (n - 1) * stride;
                        T *d = data + offset;

                        if (cross_block==2 and axis_begin+n*axis_stride<global_dimensions[cur_axis]){
                            recover(d - data, *d, interp_linear(*(d - stride), *(d + stride)) );

                        }
                        else if (n >= 4 or (cross_block and  axis_begin >= (4-n)*axis_stride )) {
                                
                            recover(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride)) );
                              
                                  
                        } 
                        else { 
                            recover(d - data, *d, *(d - stride) );
                        }
                    }
                }
            } else {
                if (pb == PB_predict_overwrite) {

                    if(tuning){
                        T *d;
                        size_t i;
                        for (i = 3; i + 3 < n; i += 2) {
                            d = data + begin + i * stride;
                            
                            predict_error+=quantize_tuning(d - data, *d,
                                     interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)),tuning);
                           
                        }
                        d = data + begin + stride;
                        if(cross_block and axis_begin >= 2*axis_stride){
                            if(axis_begin+4*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+4*stride<end) )
                                predict_error+=quantize_tuning(d - data, *d,
                                    interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)),tuning);
                            else if (axis_begin+2*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+2*stride<end))
                                predict_error+=quantize_tuning(d - data, *d,
                                    interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride) ),tuning);
                            else
                                predict_error+=quantize_tuning(d - data, *d,
                                    interp_linear1(*(d - stride3x), *(d - stride) ),tuning);
                            
                        }
                        else{
                            if(axis_begin+4*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+4*stride<end) )
                                predict_error+=quantize_tuning(d - data, *d, interp_quad_1(*(d - stride), *(d + stride), *(d + stride3x)),tuning);
                            else if (axis_begin+2*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+2*stride<end) )
                                predict_error+=quantize_tuning(d - data, *d, interp_linear(*(d - stride), *(d + stride) ),tuning);
                            else
                                predict_error+=quantize_tuning(d - data, *d, *(d - stride) ,tuning);
                        }
                       
                        
                        
                        if (begin + i * stride<end){
                            d = data + begin + i * stride;

                            if(cross_block==2 and axis_begin+(i+3)*axis_stride<global_dimensions[cur_axis] and axis_begin+(i-3)*axis_stride>=0)
                                predict_error+=quantize_tuning(d - data, *d,
                                         interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)),tuning);
                            else if (axis_begin+(i-3)*axis_stride>=0 and (cross_block>0 or i>=3) ){
                                if (axis_begin+(i+1)*axis_stride<global_dimensions[cur_axis] ){
                                    
                                    predict_error+=quantize_tuning(d - data, *d, interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)),tuning );
                                }
                                else if (cross_block>0 or i>=3){
                                   
                                    predict_error+=quantize_tuning(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride) ),tuning );
                                }
                                else{
                                    predict_error+=quantize_tuning(d - data, *d, *(d - stride) ,tuning );
                                }

                            }
                            else{
                                if (axis_begin+(i+1)*axis_stride<global_dimensions[cur_axis]){
                                    
                                    predict_error+=quantize_tuning(d - data, *d, interp_linear( *(d - stride), *(d + stride)),tuning );
                                }
                                else{
                                    
                                    predict_error+=quantize_tuning(d - data, *d, *(d - stride),tuning );
                                }

                            }
                        }


                        
                        if (n % 2 == 0 and n>4) {
                            size_t offset=begin + (n - 1) * stride;
                            d = data + offset;
                            if(cross_block==2 and axis_begin+(n+2)*axis_stride<global_dimensions[cur_axis] and axis_begin+(n-4)*axis_stride>=0)
                                predict_error+=quantize_tuning(d - data, *d,
                                     interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)),tuning);
                            else if (cross_block==2 and axis_begin+n*axis_stride<global_dimensions[cur_axis] and axis_begin+(n-4)*axis_stride>=0)
                                predict_error+=quantize_tuning(d - data, *d, interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)),tuning);
                            else if (cross_block==2 and axis_begin+n*axis_stride<global_dimensions[cur_axis])
                                predict_error+=quantize_tuning(d - data, *d, interp_linear( *(d - stride), *(d + stride)),tuning);
                            else if (axis_begin >= (6-n)*axis_stride and (cross_block>0 or n>=6) )
                                predict_error+=quantize_tuning(d - data, *d, interp_quad_3(*(d - stride5x), *(d - stride3x), *(d - stride)),tuning);
                            else if (axis_begin >= (4-n)*axis_stride and (cross_block>0 or n>=4) )
                                predict_error+=quantize_tuning(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride)),tuning);
                            else
                                predict_error+=quantize_tuning(d - data, *d, *(d - stride),tuning);

                           


                        }
                    }

                    
                    else{
                        

                        T *d;
                        size_t i;
                        for (i = 3; i + 3 < n; i += 2) {
                            d = data + begin + i * stride;
                            //if(mark[begin+i*stride])
                                //std::cout<<"ne1 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;

                            quantize(d - data, *d,
                                     interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)) );
                            /*
                            mark[begin + i * stride]=true;
                            if(!mark[begin+(i-3)*stride])
                                std::cout<<"e1 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                            if(!mark[begin+(i-1)*stride])
                                std::cout<<"e2 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                            if(!mark[begin+(i+1)*stride])
                                std::cout<<"e3 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                            if(!mark[begin+(i+3)*stride])
                                std::cout<<"e4 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                */
                        }
                        d = data + begin + stride;
                        //if(mark[begin+stride])
                            //std::cout<<"ne2 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                        if(cross_block and axis_begin >= 2*axis_stride){
                            if(axis_begin+4*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+4*stride<end) ){
                                quantize(d - data, *d,
                                    interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)));
                                /*
                                if(!mark[begin-2*stride])
                                    std::cout<<"e5 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin])
                                    std::cout<<"e6 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+2*stride])
                                    std::cout<<"e7 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+4*stride])
                                    std::cout<<"e8 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                */
                            }
                            else if (axis_begin+2*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+2*stride<end)) {
                                quantize(d - data, *d,
                                    interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride) ));
                                /*
                                if(!mark[begin-2*stride])
                                    std::cout<<"e9 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin])
                                    std::cout<<"e10 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+2*stride])
                                    std::cout<<"e11 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                */
                                
                            }
                            else{
                                quantize(d - data, *d,
                                    interp_linear1(*(d - stride3x), *(d - stride) ) );
                                /*
                                if(!mark[begin-2*stride])
                                    std::cout<<"e11.3 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin])
                                    std::cout<<"e11.6 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                */
                            }
                            
                        }


                        else{
                            if(axis_begin+4*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+4*stride<end) ){
                                quantize(d - data, *d, interp_quad_1(*(d - stride), *(d + stride), *(d + stride3x)) );
                                /*
                                if(!mark[begin])
                                    std::cout<<"e12 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+2*stride])
                                    std::cout<<"e13 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+4*stride])
                                    std::cout<<"e14 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                */
                            }
                            else if (axis_begin+2*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+2*stride<end) ){
                                quantize(d - data, *d, interp_linear(*(d - stride), *(d + stride) ));
                                /*
                                if(!mark[begin])
                                    std::cout<<"e15 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+2*stride])
                                    std::cout<<"e16 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                */
                            }
                            else{
                                quantize(d - data, *d, *(d - stride) );
                                //if(!mark[begin])
                                    //std::cout<<"e16.5 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                            }
                        }


                       // mark[begin+stride]=true;
                        if (begin + i * stride<end){
                            //if(mark[begin+i*stride])
                              //  std::cout<<"ne3 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                            d = data + begin + i * stride;
                            if(cross_block==2 and axis_begin+(i+3)*axis_stride<global_dimensions[cur_axis] and axis_begin+(i-3)*axis_stride>=0){
                                quantize(d - data, *d,
                                         interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)) );
                                /*
                                if(!mark[begin+(i-3)*stride])
                                    std::cout<<"e17 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+(i-1)*stride])
                                    std::cout<<"e18 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+(i+1)*stride])
                                    std::cout<<"e19 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+(i+3)*stride] and tuning==0){

                                    
                                    size_t temp=begin;
                                    size_t x =temp/(352*1008);
                                    temp=temp % (352*1008);
                                    size_t y = temp/352;
                                    size_t z =temp %352;
                                    std::cout<<"---"<<std::endl;
                                    std::cout<<"e20 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                    std::cout<<"e20 "<<x<<" "<<y<<" "<<z<<" "<<cur_axis<<std::endl;
                                    std::cout<<"---"<<std::endl;
                                }
                                */


                            }
                            else if (axis_begin+(i-3)*axis_stride>=0 and (cross_block>0 or i>=3)){
                                if (axis_begin+(i+1)*axis_stride<global_dimensions[cur_axis]){
                                    
                                    quantize(d - data, *d, interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)) );
                                    /*
                                    if(!mark[begin+(i-3)*stride])
                                        std::cout<<"e21 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                    if(!mark[begin+(i-1)*stride])
                                        std::cout<<"e22 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                    if(!mark[begin+(i+1)*stride])
                                        std::cout<<"e23 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                    */
                                }
                                else if (cross_block>0 or i>=3){
                                   
                                    quantize(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride) ) );
                                    /*
                                    if(!mark[begin+(i-3)*stride])
                                        std::cout<<"e24 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                    if(!mark[begin+(i-1)*stride])
                                        std::cout<<"e25 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                    */
                                }
                                else{
                                    quantize(d - data, *d,  *(d - stride)  );
                                   // if(!mark[begin+(i-1)*stride])
                                        //std::cout<<"e25.5 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;

                                }

                            }
                            else{
                                if (axis_begin+(i+1)*axis_stride<global_dimensions[cur_axis]){
                                    
                                    quantize(d - data, *d, interp_linear( *(d - stride), *(d + stride)) );
                                    /*
                                    if(!mark[begin+(i-1)*stride])
                                        std::cout<<"e26 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                    if(!mark[begin+(i+1)*stride])
                                        std::cout<<"e27 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                    */
                                }
                                else{
                                    
                                    quantize(d - data, *d, *(d - stride) );
                                    //if(!mark[begin+(i-1)*stride])
                                        //std::cout<<"e28 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                }

                            }
                            //mark[begin+i*stride]=true;
                        }
                        if (n % 2 == 0 and n>4) {
                            size_t offset=begin + (n - 1) * stride;
                            d = data + offset;
                            //if(mark[offset])
                               // std::cout<<"ne4 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                            if(cross_block==2 and axis_begin+(n+2)*axis_stride<global_dimensions[cur_axis] and axis_begin+(n-4)*axis_stride>=0){
                                quantize(d - data, *d,
                                     interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)) );
                                /*
                                if(!mark[begin+(n-4)*stride])
                                    std::cout<<"e29 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+(n-2)*stride])
                                    std::cout<<"e30 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+n*stride])
                                    std::cout<<"e31 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+(n+2)*stride])
                                    std::cout<<"e32 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                */


                            }
                            else if (cross_block==2 and axis_begin+n*axis_stride<global_dimensions[cur_axis] and axis_begin+(n-4)*axis_stride>=0){
                                quantize(d - data, *d, interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)) );
                                /*
                                if(!mark[begin+(n-4)*stride])
                                    std::cout<<"e33 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+(n-2)*stride])
                                    std::cout<<"e34 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+n*stride])
                                    std::cout<<"e35 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                */
                            }
                            else if (cross_block==2 and axis_begin+n*axis_stride<global_dimensions[cur_axis]){
                                quantize(d - data, *d, interp_linear( *(d - stride), *(d + stride)) );
                                /*
                                if(!mark[begin+(n-2)*stride])
                                    std::cout<<"e36 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+n*stride])
                                    std::cout<<"e37 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                */


                            }
                            else if (axis_begin >= (6-n)*axis_stride and (cross_block>0 or n>=6)){
                                quantize(d - data, *d, interp_quad_3(*(d - stride5x), *(d - stride3x), *(d - stride)) );
                                /*
                                if(!mark[begin+(n-6)*stride])
                                    std::cout<<"e38 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;

                                if(!mark[begin+(n-4)*stride])
                                    std::cout<<"e39 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+(n-2)*stride])
                                    std::cout<<"e40 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                */


                            }
                            else if (axis_begin >= (4-n)*axis_stride and (cross_block>0 or n>=4) ){
                                quantize(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride)) );
                                /*
                                if(!mark[begin+(n-4)*stride])
                                    std::cout<<"e41 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                if(!mark[begin+(n-2)*stride])
                                    std::cout<<"e42 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                                */
                            }
                            else{
                                quantize(d - data, *d, *(d - stride) );
                                //if(!mark[begin+(n-2)*stride])
                                    //std::cout<<"e43 "<<axis_begin<<" "<<i<<" "<<axis_stride<<" "<<global_dimensions[cur_axis]<<std::endl;
                            }

                           // mark[offset]=true;


                        }

                        
                    }

                } else {

                    T *d;
                    size_t i;
                   // std::cout<<"zunnnihuojia1"<<std::endl;
                    for (i = 3; i + 3 < n; i += 2) {
                        d = data + begin + i * stride;
                        recover(d - data, *d,
                            interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)) );
                    }
                    d = data + begin + stride;
                    if(cross_block and axis_begin >= 2*axis_stride){
                        if(axis_begin+4*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+4*stride<end) ){
                            //std::cout<<"zunnnihuojia2"<<std::endl;
                            recover(d - data, *d,
                                interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)));
                        }
                        else if (axis_begin+2*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+2*stride<end)) {
                            //std::cout<<"zunnnihuojia2.5"<<std::endl;
                            recover(d - data, *d,
                                interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride) ));
                        }
                        else{
                            //std::cout<<"zunnnihuojia2.7"<<std::endl;
                             recover(d - data, *d,
                                interp_linear1(*(d - stride3x), *(d - stride) ));
                            

                        }
                    }
                    else{
                        if(axis_begin+4*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+4*stride<end) ){
                           // std::cout<<"zunnnihuojia3"<<std::endl;
                            recover(d - data, *d, interp_quad_1(*(d - stride), *(d + stride), *(d + stride3x)));
                        }
                        else if(axis_begin+2*axis_stride<global_dimensions[cur_axis] and (cross_block==2 or begin+2*stride<end) ){
                            //std::cout<<"zunnnihuojia3.5"<<std::endl;
                            recover(d - data, *d, interp_linear(*(d - stride), *(d + stride) ));
                        }
                        else{
                            //std::cout<<"zunnnihuojia3.75"<<std::endl;
                            recover(d - data, *d, *(d - stride));
                        }
                    }

                    
                    if (begin + i * stride<end){
                        d = data + begin + i * stride;
                        if(cross_block==2 and axis_begin+(i+3)*axis_stride<global_dimensions[cur_axis] and axis_begin+(i-3)*axis_stride>=0){
                            //std::cout<<"zunnnihuojia4"<<std::endl;
                            recover(d - data, *d,
                                    interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)) );
                        }
                        else if (axis_begin+(i-3)*axis_stride>=0 and (cross_block>0 or i>=3) ){
                            if (axis_begin+(i+1)*axis_stride<global_dimensions[cur_axis]){
                                //std::cout<<"zunnnihuojia5"<<std::endl;
                                recover(d - data, *d, interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)) );
                            }
                            else if (cross_block>0 or i>=3){
                                //std::cout<<"zunnnihuojia5.25"<<std::endl;
                                recover(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride) ) );
                            }
                            else{
                                recover(d - data, *d,  *(d - stride)  );
                            }

                        }
                        else{
                            if (axis_begin+(i+1)*axis_stride<global_dimensions[cur_axis]){
                                //std::cout<<"zunnnihuojia5.5"<<std::endl;
                                recover(d - data, *d, interp_linear( *(d - stride), *(d + stride)) );
                            }
                            else{
                                //std::cout<<"zunnnihuojia5.75"<<std::endl;
                                recover(d - data, *d, *(d - stride) );
                            }

                        }
                    }
                    if (n % 2 == 0 and n>4) {

                        size_t offset=begin + (n - 1) * stride;
                        d = data + offset;
                        if(cross_block==2 and axis_begin+(n+2)*axis_stride<global_dimensions[cur_axis] and axis_begin+(n-4)*axis_stride>=0){
                            //std::cout<<"zunnnihuojia6"<<std::endl;
                            recover(d - data, *d,
                                interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)) );
                        }
                        else if (cross_block==2 and axis_begin+n*axis_stride<global_dimensions[cur_axis] and axis_begin+(n-4)*axis_stride>=0){
                            //std::cout<<"zunnnihuojia7"<<std::endl;
                            recover(d - data, *d, interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)) );
                        }
                        else if (cross_block==2 and axis_begin+n*axis_stride<global_dimensions[cur_axis]){
                            //std::cout<<"zunnnihuojia7.5"<<std::endl;
                            recover(d - data, *d, interp_linear( *(d - stride), *(d + stride)) );

                        }
                        else if (axis_begin >= (6-n)*axis_stride and (cross_block>0 or n>=6) ){
                           // std::cout<<"zunnnihuojia8"<<std::endl;
                            recover(d - data, *d, interp_quad_3(*(d - stride5x), *(d - stride3x), *(d - stride)) );
                        }
                        else if (axis_begin >= (4-n)*axis_stride and (cross_block>0 or n>=4) ){
                            //std::cout<<"zunnnihuojia9"<<std::endl;
                            recover(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride)) );
                        }
                        else{
                            //std::cout<<"zunnnihuojia10"<<std::endl;
                            recover(d - data, *d, *(d - stride) );
                        }

                    }
                }
            }

            return predict_error;
        }
        
        double block_interpolation_1d(T *data, size_t begin, size_t end, size_t stride,
                                      const std::string &interp_func,
                                      const PredictorBehavior pb,int tuning=0) {

            size_t n = (end - begin) / stride + 1;
            if (n <= 1) {
                return 0;
            }
            double predict_error = 0;

            size_t stride3x = 3 * stride;
            size_t stride5x = 5 * stride;

            if (interp_func == "linear" || n < 5) {
                if (pb == PB_predict_overwrite) {

                    if (tuning){
                        for (size_t i = 1; i + 1 < n; i += 2) {
                            T *d = data + begin + i * stride;
                            predict_error+=quantize_tuning(d - data, *d, interp_linear(*(d - stride), *(d + stride)),tuning);
                        }
                        if (n % 2 == 0) {
                            T *d = data + begin + (n - 1) * stride;
                            if (n < 4) {
                                predict_error+=quantize_tuning(d - data, *d, *(d - stride),tuning);
                            } else {
                                predict_error+=quantize_tuning(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride)),tuning);
                            }
                        }

                    }
                    else{
                       

                        for (size_t i = 1; i + 1 < n; i += 2) {
                            T *d = data + begin + i * stride;
                            quantize(d - data, *d, interp_linear(*(d - stride), *(d + stride)));
                        }
                      

                        if (n % 2 == 0) {
                            T *d = data + begin + (n - 1) * stride;
                            if (n < 4) {
                                
                              
                                quantize(d - data, *d, *(d - stride));
                               

                            } else {
                               

                                quantize(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride)));
                                

                            }
                        }
                       

                    }

                } else {
                    for (size_t i = 1; i + 1 < n; i += 2) {
                        T *d = data + begin + i * stride;
                        recover(d - data, *d, interp_linear(*(d - stride), *(d + stride)));
                    }
                    if (n % 2 == 0) {
                        T *d = data + begin + (n - 1) * stride;
                        if (n < 4) {
                            recover(d - data, *d, *(d - stride));
                        } else {
                            recover(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride)));
                        }
                    }
                }
            } else {
                if (pb == PB_predict_overwrite) {

                    if(tuning){
                        T *d;
                        size_t i;
                        for (i = 3; i + 3 < n; i += 2) {
                            d = data + begin + i * stride;
                            predict_error+=quantize_tuning(d - data, *d,
                                     interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)),tuning);
                        }
                        d = data + begin + stride;
                        predict_error+=quantize_tuning(d - data, *d, interp_quad_1(*(d - stride), *(d + stride), *(d + stride3x)),tuning);

                        d = data + begin + i * stride;
                        predict_error+=quantize_tuning(d - data, *d, interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)),tuning);
                        if (n % 2 == 0) {
                            d = data + begin + (n - 1) * stride;
                            predict_error+=quantize_tuning(d - data, *d, interp_quad_3(*(d - stride5x), *(d - stride3x), *(d - stride)),tuning);
                        }
                    }

                    
                    else{
                        

                        T *d;
                        size_t i;
                        for (i = 3; i + 3 < n; i += 2) {
                            d = data + begin + i * stride;
                            quantize(d - data, *d,
                                     interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)));
                        }
                       

                        d = data + begin + stride;
                        quantize(d - data, *d, interp_quad_1(*(d - stride), *(d + stride), *(d + stride3x)));

                        d = data + begin + i * stride;
                        
                        quantize(d - data, *d, interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)));
                        
                        if (n % 2 == 0) {
                            d = data + begin + (n - 1) * stride;
                            quantize(d - data, *d, interp_quad_3(*(d - stride5x), *(d - stride3x), *(d - stride)));
                        }
                        
                    }

                } else {
                    T *d;

                    size_t i;
                    for (i = 3; i + 3 < n; i += 2) {
                        d = data + begin + i * stride;
                        recover(d - data, *d, interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)));
                    }
                    d = data + begin + stride;

                    recover(d - data, *d, interp_quad_1(*(d - stride), *(d + stride), *(d + stride3x)));

                    d = data + begin + i * stride;
                    recover(d - data, *d, interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)));

                    if (n % 2 == 0) {
                        d = data + begin + (n - 1) * stride;
                        recover(d - data, *d, interp_quad_3(*(d - stride5x), *(d - stride3x), *(d - stride)));
                    }
                }
            }

            return predict_error;
        }
        
        /*
        double block_interpolation_1d_2(T *data, size_t begin, size_t end, size_t stride,
                                      const std::string &interp_func,
                                      const PredictorBehavior pb) {

            size_t n = (end - begin) / stride + 1;
            if (n <= 1) {
                return 0;
            }
            double predict_error = 0;

            size_t stride3x = 3 * stride;
            size_t stride5x = 5 * stride;

            if (interp_func == "linear" || n < 5) {
                if (pb == PB_predict_overwrite) {

                    
                       

                        for (size_t i = 1; i + 1 < n; i += 2) {
                            T *d = data + begin + i * stride;
                            quantize(d - data, *d, interp_linear(*(d - stride), *(d + stride)));
                        }
                      

                        if (n % 2 == 0) {
                            T *d = data + begin + (n - 1) * stride;
                            if (n < 4) {
                                
                              
                                quantize(d - data, *d, *(d - stride));
                               

                            } else {
                               

                                quantize(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride)));
                                

                            }
                        }
                       

                    

                } else {
                    for (size_t i = 1; i + 1 < n; i += 2) {
                        T *d = data + begin + i * stride;
                        recover(d - data, *d, interp_linear(*(d - stride), *(d + stride)));
                    }
                    if (n % 2 == 0) {
                        T *d = data + begin + (n - 1) * stride;
                        if (n < 4) {
                            recover(d - data, *d, *(d - stride));
                        } else {
                            recover(d - data, *d, interp_linear1(*(d - stride3x), *(d - stride)));
                        }
                    }
                }
            } else {
                if (pb == PB_predict_overwrite) {

                    

                    
                    
                        

                        T *d;
                        size_t i;
                        for (i = 3; i + 3 < n; i += 2) {
                            d = data + begin + i * stride;
                            quantize(d - data, *d,
                                     interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)));
                        }
                       

                        d = data + begin + stride;
                        quantize(d - data, *d, interp_quad_1(*(d - stride), *(d + stride), *(d + stride3x)));

                        d = data + begin + i * stride;
                        
                        quantize(d - data, *d, interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)));
                        
                        if (n % 2 == 0) {
                            d = data + begin + (n - 1) * stride;
                            quantize(d - data, *d, interp_quad_3(*(d - stride5x), *(d - stride3x), *(d - stride)));
                        }
                        
                    

                } else {
                    T *d;
                    //QoZ::Timer timer(true);

                    size_t i;
                    for (i = 3; i + 3 < n; i += 2) {
                        d = data + begin + i * stride;
                        recover(d - data, *d, interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)));
                    }
                    d = data + begin + stride;

                    recover(d - data, *d, interp_quad_1(*(d - stride), *(d + stride), *(d + stride3x)));

                    d = data + begin + i * stride;
                    recover(d - data, *d, interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)));

                    if (n % 2 == 0) {
                        d = data + begin + (n - 1) * stride;
                        recover(d - data, *d, interp_quad_3(*(d - stride5x), *(d - stride3x), *(d - stride)));
                    }
                   // if ((begin==0 or begin==512 or begin==512*512) and (stride==1 or stride==512 or stride==512*512) )
                        //timer.stop("Recover");
                }
            }

            return predict_error;
        }

        */


        double block_interpolation_2d(T *data, size_t begin1, size_t end1, size_t begin2, size_t end2, size_t stride1,size_t stride2,
                                      const std::string &interp_func,
                                      const PredictorBehavior pb,int tuning=0) {
            size_t n = (end1 - begin1) / stride1 + 1;
            if (n <= 1) {
                return 0;
            }


            size_t m = (end2 - begin2) / stride2 + 1;
            if (m <= 1) {
                return 0;
            }
            double predict_error = 0;


            //size_t stride3x = 3 * stride;
            //size_t stride5x = 5 * stride;

            {
                if (pb == PB_predict_overwrite) {

                    if (tuning){
                        for (size_t i = 1; i + 1 < n; i += 2) {
                            for(size_t j=1;j+1<m;j+=2){
                                T *d = data + begin1 + i* stride1+begin2+j*stride2;
                                predict_error+=quantize_tuning(d - data, *d, interp_2d(*(d - stride1), *(d + stride1),*(d - stride2), *(d + stride2)),tuning);

                            }
                            if(m%2 ==0){
                                T *d = data + begin1 + i * stride1+begin2+(m-1)*stride2;
                                predict_error+=quantize_tuning(d - data, *d, interp_linear(*(d - stride1), *(d + stride1)),tuning);


                            }


                           
                        }
                        if (n % 2 == 0) {
                            for(size_t j=1;j+1<m;j+=2){
                                T *d = data + begin1 + (n-1) * stride1+begin2+j*stride2;
                                predict_error+=quantize_tuning(d - data, *d, interp_linear(*(d - stride2), *(d + stride2)),tuning);

                            }
                            if(m%2 ==0){
                                T *d = data + begin1 + (n-1) * stride1+begin2+(m-1)*stride2;
                                predict_error+=quantize_tuning(d - data, *d, lorenzo_2d(*(d - stride1-stride2), *(d - stride1), *(d - stride2)),tuning);
                            }
                            
                        }

                    }





                    else{
                        for (size_t i = 1; i + 1 < n; i += 2) {
                            for(size_t j=1;j+1<m;j+=2){
                                T *d = data + begin1 + i * stride1+begin2+j*stride2;
                                quantize(d - data, *d, interp_2d(*(d - stride1), *(d + stride1),*(d - stride2), *(d + stride2)));

                            }
                            if(m%2 ==0){
                                T *d = data + begin1 + i * stride1+begin2+(m-1)*stride2;
                                quantize(d - data, *d, interp_linear(*(d - stride1), *(d + stride1)));


                            }


                           
                        }
                        if (n % 2 == 0) {
                            for(size_t j=1;j+1<m;j+=2){
                                T *d = data + begin1 + (n-1) * stride1+begin2+j*stride2;
                                quantize(d - data, *d, interp_linear(*(d - stride2), *(d + stride2)));

                            }
                            if(m%2 ==0){
                                T *d = data + begin1 + (n-1) * stride1+begin2+(m-1)*stride2;
                                quantize(d - data, *d, lorenzo_2d(*(d - stride1-stride2), *(d - stride1), *(d - stride2)));
                            }
                            
                        }
                    }

                } else {

                    for (size_t i = 1; i + 1 < n; i += 2) {
                            for(size_t j=1;j+1<m;j+=2){
                                T *d = data + begin1 + i * stride1+begin2+j*stride2;
                                recover(d - data, *d, interp_2d(*(d - stride1), *(d + stride1),*(d - stride2), *(d + stride2)));

                            }
                            if(m%2 ==0){
                                T *d = data + begin1 + i * stride1+begin2+(m-1)*stride2;
                                recover(d - data, *d, interp_linear(*(d - stride1), *(d + stride1)));


                            }


                           
                        }
                        if (n % 2 == 0) {
                            for(size_t j=1;j+1<m;j+=2){
                                T *d = data + begin1 + (n-1) * stride1+begin2+j*stride2;
                                recover(d - data, *d, interp_linear(*(d - stride2), *(d + stride2)));

                            }
                            if(m%2 ==0){
                                T *d = data + begin1 + (n-1) * stride1+begin2+(m-1)*stride2;
                                recover(d - data, *d, lorenzo_2d(*(d - stride1-stride2), *(d - stride1), *(d - stride2)));
                            }
                            
                        }
                }
            


            }
            return predict_error;
        }
        
        double block_interpolation_3d(T *data, size_t begin1, size_t end1, size_t begin2, size_t end2, size_t begin3, size_t end3, size_t stride1,size_t stride2,size_t stride3,
                                      const std::string &interp_func,
                                      const PredictorBehavior pb,int tuning=0) {
            size_t n = (end1 - begin1) / stride1 + 1;
            if (n <= 1) {
                return 0;
            }


            size_t m = (end2 - begin2) / stride2 + 1;
            if (m <= 1) {
                return 0;
            }
            size_t p = (end3 - begin3) / stride3 + 1;
            if (p <= 1) {
                return 0;
            }
            double predict_error = 0;


            //size_t stride3x = 3 * stride;
            //size_t stride5x = 5 * stride;

            {
                if (pb == PB_predict_overwrite) {

                    if (tuning){
                        for (size_t i = 1; i + 1 < n; i += 2) {
                            for(size_t j=1;j+1<m;j+=2){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + i* stride1+begin2+j*stride2+begin3+k*stride3;
                                    predict_error+=quantize_tuning(d - data, *d, interp_3d(*(d - stride1), *(d + stride1),*(d - stride2), *(d + stride2),*(d - stride3), *(d + stride3)),tuning);
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + i* stride1+begin2+j*stride2+begin3+(p-1)*stride3;
                                    predict_error+=quantize_tuning(d - data, *d, interp_2d(*(d - stride1), *(d + stride1),*(d - stride2), *(d + stride2)),tuning);
                                }

                            }
                            if(m%2 ==0){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + i* stride1+begin2+(m-1)*stride2+begin3+k*stride3;
                                    predict_error+=quantize_tuning(d - data, *d, interp_2d(*(d - stride1), *(d + stride1),*(d - stride3), *(d + stride3)),tuning);
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + i* stride1+begin2+(m-1)*stride2+begin3+(p-1)*stride3;
                                    predict_error+=quantize_tuning(d - data, *d, interp_linear(*(d - stride1), *(d + stride1)),tuning);
                                }
                            }


                           
                        }
                        if (n % 2 == 0) {
                            for(size_t j=1;j+1<m;j+=2){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+j*stride2+begin3+k*stride3;
                                    predict_error+=quantize_tuning(d - data, *d, interp_2d(*(d - stride2), *(d + stride2),*(d - stride3), *(d + stride3)),tuning);
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+j*stride2+begin3+(p-1)*stride3;
                                    predict_error+=quantize_tuning(d - data, *d, interp_linear(*(d - stride2), *(d + stride2)),tuning);
                                }



                            }
                            if(m%2 ==0){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+(m-1)*stride2+begin3+k*stride3;
                                    predict_error+=quantize_tuning(d - data, *d, interp_linear(*(d - stride3), *(d + stride3)),tuning);
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+(m-1)*stride2+begin3+(p-1)*stride3;
                                    predict_error+=quantize_tuning(d - data, *d, lorenzo_3d(*(d-stride1-stride2-stride3),*(d-stride1-stride2),*(d-stride1-stride3),*(d-stride1),*(d-stride2-stride3),*(d-stride2),*(d-stride3)),tuning);
                                }
                            }
                            
                        }

                    }





                    else{
                        for (size_t i = 1; i + 1 < n; i += 2) {
                            for(size_t j=1;j+1<m;j+=2){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + i* stride1+begin2+j*stride2+begin3+k*stride3;
                                    quantize(d - data, *d, interp_3d(*(d - stride1), *(d + stride1),*(d - stride2), *(d + stride2),*(d - stride3), *(d + stride3)));
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + i* stride1+begin2+j*stride2+begin3+(p-1)*stride3;
                                    quantize(d - data, *d, interp_2d(*(d - stride1), *(d + stride1),*(d - stride2), *(d + stride2)));
                                }

                            }
                            if(m%2 ==0){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + i* stride1+begin2+(m-1)*stride2+begin3+k*stride3;
                                    quantize(d - data, *d, interp_2d(*(d - stride1), *(d + stride1),*(d - stride3), *(d + stride3)));
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + i* stride1+begin2+(m-1)*stride2+begin3+(p-1)*stride3;
                                    quantize(d - data, *d, interp_linear(*(d - stride1), *(d + stride1)));
                                }
                            }


                           
                        }
                        if (n % 2 == 0) {
                            for(size_t j=1;j+1<m;j+=2){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+j*stride2+begin3+k*stride3;
                                    quantize(d - data, *d, interp_2d(*(d - stride2), *(d + stride2),*(d - stride3), *(d + stride3)) );
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+j*stride2+begin3+(p-1)*stride3;
                                    quantize(d - data, *d, interp_linear(*(d - stride2), *(d + stride2)));
                                }



                            }
                            if(m%2 ==0){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+(m-1)*stride2+begin3+k*stride3;
                                    quantize(d - data, *d, interp_linear(*(d - stride3), *(d + stride3)));
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+(m-1)*stride2+begin3+(p-1)*stride3;
                                    quantize(d - data, *d, lorenzo_3d(*(d-stride1-stride2-stride3),*(d-stride1-stride2),*(d-stride1-stride3),*(d-stride1),*(d-stride2-stride3),*(d-stride2),*(d-stride3)));
                                }
                            }
                            
                        }
                    }

                } else {

                    for (size_t i = 1; i + 1 < n; i += 2) {
                            for(size_t j=1;j+1<m;j+=2){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + i* stride1+begin2+j*stride2+begin3+k*stride3;
                                    recover(d - data, *d, interp_3d(*(d - stride1), *(d + stride1),*(d - stride2), *(d + stride2),*(d - stride3), *(d + stride3)));
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + i* stride1+begin2+j*stride2+begin3+(p-1)*stride3;
                                    recover(d - data, *d, interp_2d(*(d - stride1), *(d + stride1),*(d - stride2), *(d + stride2)));
                                }

                            }
                            if(m%2 ==0){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + i* stride1+begin2+(m-1)*stride2+begin3+k*stride3;
                                    recover(d - data, *d, interp_2d(*(d - stride1), *(d + stride1),*(d - stride3), *(d + stride3)));
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + i* stride1+begin2+(m-1)*stride2+begin3+(p-1)*stride3;
                                    recover(d - data, *d, interp_linear(*(d - stride1), *(d + stride1)));
                                }
                            }


                           
                        }
                        if (n % 2 == 0) {
                            for(size_t j=1;j+1<m;j+=2){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+j*stride2+begin3+k*stride3;
                                    recover(d - data, *d, interp_2d(*(d - stride2), *(d + stride2),*(d - stride3), *(d + stride3)) );
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+j*stride2+begin3+(p-1)*stride3;
                                    recover(d - data, *d, interp_linear(*(d - stride2), *(d + stride2)));
                                }



                            }
                            if(m%2 ==0){
                                for(size_t k=1;k+1<p;k+=2){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+(m-1)*stride2+begin3+k*stride3;
                                    recover(d - data, *d, interp_linear(*(d - stride3), *(d + stride3)));
                                }
                                if(p%2==0){
                                    T *d = data + begin1 + (n-1)* stride1+begin2+(m-1)*stride2+begin3+(p-1)*stride3;
                                    recover(d - data, *d, lorenzo_3d(*(d-stride1-stride2-stride3),*(d-stride1-stride2),*(d-stride1-stride3),*(d-stride1),*(d-stride2-stride3),*(d-stride2),*(d-stride3)));
                                }
                            }
                            
                        }
                }
            


            }
            return predict_error;
        }
        







        template<uint NN = N>
        typename std::enable_if<NN == 1, double>::type
        block_interpolation(T *data, std::array<size_t, N> begin, std::array<size_t, N> end, const PredictorBehavior pb,
                            const std::string &interp_func, const int direction, uint stride = 1,int tuning=0,size_t cross_block=0) {
            return block_interpolation_1d(data, begin[0], end[0], stride, interp_func, pb,tuning);
        }


        template<uint NN = N>
        typename std::enable_if<NN == 2, double>::type
        block_interpolation(T *data, std::array<size_t, N> begin, std::array<size_t, N> end, const PredictorBehavior pb,
                            const std::string &interp_func, const int direction, uint stride = 1,int tuning=0,size_t cross_block=0) {
            double predict_error = 0;
            size_t stride2x = stride * 2;

            //if(direction!=2){
               
                const std::array<int, N> dims = dimension_sequences[direction];
                for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride2x : 0); j <= end[dims[1]]; j += stride2x) {
                    size_t begin_offset = begin[dims[0]] * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]];
                    predict_error += block_interpolation_1d(data, begin_offset,
                                                            begin_offset +
                                                            (end[dims[0]] - begin[dims[0]]) * dimension_offsets[dims[0]],
                                                            stride * dimension_offsets[dims[0]], interp_func, pb,tuning);
                }
                for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride : 0); i <= end[dims[0]]; i += stride) {
                    size_t begin_offset = i * dimension_offsets[dims[0]] + begin[dims[1]] * dimension_offsets[dims[1]];
                    predict_error += block_interpolation_1d(data, begin_offset,
                                                            begin_offset +
                                                            (end[dims[1]] - begin[dims[1]]) * dimension_offsets[dims[1]],
                                                            stride * dimension_offsets[dims[1]], interp_func, pb,tuning);
                }
            //}
            /*
            else{
                

                const std::array<int, N> dims = dimension_sequences[0];
                for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride2x : 0); j <= end[dims[1]]; j += stride2x) {
                    size_t begin_offset = begin[dims[0]] * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]];
                    predict_error += block_interpolation_1d(data, begin_offset,
                                                            begin_offset +
                                                            (end[dims[0]] - begin[dims[0]]) * dimension_offsets[dims[0]],
                                                            stride * dimension_offsets[dims[0]], interp_func, pb,tuning);
                }
                for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride2x : 0); i <= end[dims[0]]; i += stride2x) {
                    size_t begin_offset = i * dimension_offsets[dims[0]] + begin[dims[1]] * dimension_offsets[dims[1]];
                    predict_error += block_interpolation_1d(data, begin_offset,
                                                            begin_offset +
                                                            (end[dims[1]] - begin[dims[1]]) * dimension_offsets[dims[1]],
                                                            stride * dimension_offsets[dims[1]], interp_func, pb,tuning);
                }
                size_t begin_offset1=begin[dims[0]]*dimension_offsets[dims[0]];
                size_t begin_offset2=begin[dims[1]]*dimension_offsets[dims[1]];
                //size_t stride1=dimension_offsets[dims[0]]
                predict_error+=block_interpolation_2d(data, begin_offset1,
                                                            begin_offset1 +
                                                            (end[dims[0]] - begin[dims[0]]) * dimension_offsets[dims[0]],
                                                            begin_offset2,
                                                            begin_offset2 +
                                                            (end[dims[1]] - begin[dims[1]]) * dimension_offsets[dims[1]],
                                                            stride * dimension_offsets[dims[0]],
                                                            stride * dimension_offsets[dims[1]], interp_func, pb,tuning);


               
            }
            */
            return predict_error;
        }




        template<uint NN = N>
        typename std::enable_if<NN == 3, double>::type
        block_interpolation(T *data, std::array<size_t, N> begin, std::array<size_t, N> end, const PredictorBehavior pb,
                            const std::string &interp_func, const int direction, uint stride = 1,int tuning=0,size_t cross_block=0) {//cross block: 0 or conf.num

            double predict_error = 0;
            size_t stride2x = stride * 2;
            //if(direction!=6){
            

                const std::array<int, N> dims = dimension_sequences[direction];

                if (cross_block==0){
                    for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride2x : 0); j <= end[dims[1]]; j += stride2x) {
                        for (size_t k = (begin[dims[2]] ? begin[dims[2]] + stride2x : 0); k <= end[dims[2]]; k += stride2x) {
                            size_t begin_offset = begin[dims[0]] * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]] +
                                                  k * dimension_offsets[dims[2]];
                            predict_error += block_interpolation_1d(data, begin_offset,
                                                                    begin_offset +
                                                                    (end[dims[0]] - begin[dims[0]]) *
                                                                    dimension_offsets[dims[0]],
                                                                    stride * dimension_offsets[dims[0]], interp_func, pb,tuning);
                        }
                    }
                    for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride : 0); i <= end[dims[0]]; i += stride) {
                        for (size_t k = (begin[dims[2]] ? begin[dims[2]] + stride2x : 0); k <= end[dims[2]]; k += stride2x) {
                            size_t begin_offset = i * dimension_offsets[dims[0]] + begin[dims[1]] * dimension_offsets[dims[1]] +
                                                  k * dimension_offsets[dims[2]];
                            predict_error += block_interpolation_1d(data, begin_offset,
                                                                    begin_offset +
                                                                    (end[dims[1]] - begin[dims[1]]) *
                                                                    dimension_offsets[dims[1]],
                                                                    stride * dimension_offsets[dims[1]], interp_func, pb,tuning);
                        }
                    }
                    for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride : 0); i <= end[dims[0]]; i += stride) {
                        for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride : 0); j <= end[dims[1]]; j += stride) {
                            size_t begin_offset = i * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]] +
                                                  begin[dims[2]] * dimension_offsets[dims[2]];
                            predict_error += block_interpolation_1d(data, begin_offset,
                                                                    begin_offset +
                                                                    (end[dims[2]] - begin[dims[2]]) *
                                                                    dimension_offsets[dims[2]],
                                                                    stride * dimension_offsets[dims[2]], interp_func, pb,tuning);
                        }
                    }
                }
                else{

                    for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride2x : 0); j <= end[dims[1]]; j += stride2x) {
                        for (size_t k = (begin[dims[2]] ? begin[dims[2]] + stride2x : 0); k <= end[dims[2]]; k += stride2x) {
                            size_t begin_offset = begin[dims[0]] * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]] +
                                                  k * dimension_offsets[dims[2]];
                           // std::cout<<"phase 1"<<" "<<j<<" "<<k<<std::endl;
                            predict_error += block_interpolation_1d_cross(data, begin_offset,
                                                                    begin_offset +
                                                                    (end[dims[0]] - begin[dims[0]]) *
                                                                    dimension_offsets[dims[0]],
                                                                    stride * dimension_offsets[dims[0]], interp_func, pb,tuning,2,begin[dims[0]],stride,dims[0]);
                        }
                    }
                   // size_t iidx=begin[dims[0]] ? 1: 0;
                    for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride : 0); i <= end[dims[0]]; i += stride) {
                        for (size_t k = (begin[dims[2]] ? begin[dims[2]] + stride2x : 0); k <= end[dims[2]]; k += stride2x) {
                            size_t begin_offset = i * dimension_offsets[dims[0]] + begin[dims[1]] * dimension_offsets[dims[1]] +
                                                  k * dimension_offsets[dims[2]];
                            if(i%(stride2x)==0){
                                //if(tuning==0 and stride==1)
                                   // std::cout<<"phase 2"<<" "<<i<<" "<<k<<std::endl;
                                predict_error += block_interpolation_1d_cross(data, begin_offset,
                                                                    begin_offset +
                                                                    (end[dims[1]] - begin[dims[1]]) *
                                                                    dimension_offsets[dims[1]],
                                                                    stride * dimension_offsets[dims[1]], interp_func, pb,tuning,2,begin[dims[1]],stride,dims[1]);
                            }
                            else{
                                //std::cout<<"phase 3"<<" "<<i<<" "<<k<<std::endl;
                                predict_error += block_interpolation_1d_cross(data, begin_offset,
                                                                    begin_offset +
                                                                    (end[dims[1]] - begin[dims[1]]) *
                                                                    dimension_offsets[dims[1]],
                                                                    stride * dimension_offsets[dims[1]], interp_func, pb,tuning,1,begin[dims[1]],stride,dims[1]);
                            }
                           
                        }
                        //iidx++;
                    }
                    //iidx=begin[dims[0]] ? 1: 0;
                    //size_t jjdx=begin[dims[1]] ? 1: 0;
                    for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride : 0); i <= end[dims[0]]; i += stride) {
                         for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride : 0); j <= end[dims[1]]; j += stride) {
                            size_t begin_offset = i * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]] +
                                                  begin[dims[2]] * dimension_offsets[dims[2]];
                            if(i%(stride2x)==0 and j%(stride2x)==0){
                                //if(tuning==0 and stride==1)
                                    //std::cout<<"phase 4"<<" "<<i<<" "<<j<<std::endl;
                                predict_error += block_interpolation_1d_cross(data, begin_offset,
                                                                        begin_offset +
                                                                        (end[dims[2]] - begin[dims[2]]) *
                                                                        dimension_offsets[dims[2]],
                                                                        stride * dimension_offsets[dims[2]], interp_func, pb,tuning,2,begin[dims[2]],stride,dims[2]);
                            }
                            else{
                                //std::cout<<"phase 5"<<" "<<i<<" "<<j<<std::endl;
                                predict_error += block_interpolation_1d_cross(data, begin_offset,
                                                                        begin_offset +
                                                                        (end[dims[2]] - begin[dims[2]]) *
                                                                        dimension_offsets[dims[2]],
                                                                        stride * dimension_offsets[dims[2]], interp_func, pb,tuning,1,begin[dims[2]],stride,dims[2]);
                            }


                            
                            //jjdx++;

                        }
                        //iidx++;
                    }

                }
            //}
            /*
            else{

                const std::array<int, N> dims = dimension_sequences[0];
                for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride2x : 0); j <= end[dims[1]]; j += stride2x) {
                    for (size_t k = (begin[dims[2]] ? begin[dims[2]] + stride2x : 0); k <= end[dims[2]]; k += stride2x) {
                        size_t begin_offset = begin[dims[0]] * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]] +
                                              k * dimension_offsets[dims[2]];
                        predict_error += block_interpolation_1d(data, begin_offset,
                                                                begin_offset +
                                                                (end[dims[0]] - begin[dims[0]]) *
                                                                dimension_offsets[dims[0]],
                                                                stride * dimension_offsets[dims[0]], interp_func, pb,tuning);
                    }
                }
                for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride2x : 0); i <= end[dims[0]]; i += stride2x) {
                    for (size_t k = (begin[dims[2]] ? begin[dims[2]] + stride2x : 0); k <= end[dims[2]]; k += stride2x) {
                        size_t begin_offset = i * dimension_offsets[dims[0]] + begin[dims[1]] * dimension_offsets[dims[1]] +
                                              k * dimension_offsets[dims[2]];
                        predict_error += block_interpolation_1d(data, begin_offset,
                                                                begin_offset +
                                                                (end[dims[1]] - begin[dims[1]]) *
                                                                dimension_offsets[dims[1]],
                                                                stride * dimension_offsets[dims[1]], interp_func, pb,tuning);
                    }
                }
                for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride2x : 0); i <= end[dims[0]]; i += stride2x) {
                    for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride2x : 0); j <= end[dims[1]]; j += stride2x) {
                        size_t begin_offset = i * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]] +
                                              begin[dims[2]] * dimension_offsets[dims[2]];
                        predict_error += block_interpolation_1d(data, begin_offset,
                                                                begin_offset +
                                                                (end[dims[2]] - begin[dims[2]]) *
                                                                dimension_offsets[dims[2]],
                                                                stride * dimension_offsets[dims[2]], interp_func, pb,tuning);
                    }
                }


                    for (size_t k = (begin[dims[2]] ? begin[dims[2]] + stride2x : 0); k <= end[dims[2]]; k += stride2x) {
                        size_t begin_offset1 = begin[dims[0]] * dimension_offsets[dims[0]] + k * dimension_offsets[dims[2]];
                        size_t begin_offset2 =  begin[dims[1]] * dimension_offsets[dims[1]];
                                            
                        predict_error += block_interpolation_2d(data, begin_offset1,
                                                                begin_offset1 +
                                                                (end[dims[0]] - begin[dims[0]]) *
                                                                dimension_offsets[dims[0]],
                                                                begin_offset2,
                                                                begin_offset2 +
                                                                (end[dims[1]] - begin[dims[1]]) *
                                                                dimension_offsets[dims[1]],
                                                                stride * dimension_offsets[dims[0]], stride * dimension_offsets[dims[1]],interp_func, pb,tuning);
                    }


                    for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride2x : 0); j <= end[dims[1]]; j += stride2x) {
                        size_t begin_offset1 = begin[dims[0]] * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]];
                        size_t begin_offset2 =  begin[dims[2]] * dimension_offsets[dims[2]];
                                            
                        predict_error += block_interpolation_2d(data, begin_offset1,
                                                                begin_offset1 +
                                                                (end[dims[0]] - begin[dims[0]]) *
                                                                dimension_offsets[dims[0]],
                                                                begin_offset2,
                                                                begin_offset2 +
                                                                (end[dims[2]] - begin[dims[2]]) *
                                                                dimension_offsets[dims[2]],
                                                                stride * dimension_offsets[dims[0]], stride * dimension_offsets[dims[2]],interp_func, pb,tuning);
                    }

                    for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride2x : 0); i <= end[dims[0]]; i += stride2x) {
                        size_t begin_offset1 = begin[dims[1]] * dimension_offsets[dims[1]] + i * dimension_offsets[dims[0]];
                        size_t begin_offset2 =  begin[dims[2]] * dimension_offsets[dims[2]];
                                            
                        predict_error += block_interpolation_2d(data, begin_offset1,
                                                                begin_offset1 +
                                                                (end[dims[1]] - begin[dims[1]]) *
                                                                dimension_offsets[dims[1]],
                                                                begin_offset2,
                                                                begin_offset2 +
                                                                (end[dims[2]] - begin[dims[2]]) *
                                                                dimension_offsets[dims[2]],
                                                                stride * dimension_offsets[dims[1]], stride * dimension_offsets[dims[2]],interp_func, pb,tuning);
                    }
                    size_t begin_offset1 = begin[dims[0]] * dimension_offsets[dims[0]] ;
                    size_t begin_offset2 = begin[dims[1]] * dimension_offsets[dims[1]] ;
                    size_t begin_offset3 =  begin[dims[2]] * dimension_offsets[dims[2]];
                    predict_error += block_interpolation_3d(data, begin_offset1,
                                                                begin_offset1 +
                                                                (end[dims[0]] - begin[dims[0]]) *
                                                                dimension_offsets[dims[0]],
                                                                begin_offset2,
                                                                begin_offset2 +
                                                                (end[dims[1]] - begin[dims[1]]) *
                                                                dimension_offsets[dims[1]],
                                                                begin_offset3,
                                                                begin_offset3 +
                                                                (end[dims[2]] - begin[dims[2]]) *
                                                                dimension_offsets[dims[2]],
                                                                stride * dimension_offsets[dims[0]],stride * dimension_offsets[dims[1]], stride * dimension_offsets[dims[2]],interp_func, pb,tuning);


                
                





            }*/




            return predict_error;
        }


        template<uint NN = N>
        typename std::enable_if<NN == 4, double>::type
        block_interpolation(T *data, std::array<size_t, N> begin, std::array<size_t, N> end, const PredictorBehavior pb,
                            const std::string &interp_func, const int direction, uint stride = 1,int tuning=0,size_t cross_block=0) {
            double predict_error = 0;
            size_t stride2x = stride * 2;
            max_error = 0;
            const std::array<int, N> dims = dimension_sequences[direction];
            for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride2x : 0); j <= end[dims[1]]; j += stride2x) {
                for (size_t k = (begin[dims[2]] ? begin[dims[2]] + stride2x : 0); k <= end[dims[2]]; k += stride2x) {
                    for (size_t t = (begin[dims[3]] ? begin[dims[3]] + stride2x : 0);
                         t <= end[dims[3]]; t += stride2x) {
                        size_t begin_offset =
                                begin[dims[0]] * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]] +
                                k * dimension_offsets[dims[2]] +
                                t * dimension_offsets[dims[3]];
                        predict_error += block_interpolation_1d(data, begin_offset,
                                                                begin_offset +
                                                                (end[dims[0]] - begin[dims[0]]) *
                                                                dimension_offsets[dims[0]],
                                                                stride * dimension_offsets[dims[0]], interp_func, pb,tuning);
                    }
                }
            }
//            printf("%.8f ", max_error);
            max_error = 0;
            for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride : 0); i <= end[dims[0]]; i += stride) {
                for (size_t k = (begin[dims[2]] ? begin[dims[2]] + stride2x : 0); k <= end[dims[2]]; k += stride2x) {
                    for (size_t t = (begin[dims[3]] ? begin[dims[3]] + stride2x : 0);
                         t <= end[dims[3]]; t += stride2x) {
                        size_t begin_offset =
                                i * dimension_offsets[dims[0]] + begin[dims[1]] * dimension_offsets[dims[1]] +
                                k * dimension_offsets[dims[2]] +
                                t * dimension_offsets[dims[3]];
                        predict_error += block_interpolation_1d(data, begin_offset,
                                                                begin_offset +
                                                                (end[dims[1]] - begin[dims[1]]) *
                                                                dimension_offsets[dims[1]],
                                                                stride * dimension_offsets[dims[1]], interp_func, pb,tuning);
                    }
                }
            }
//            printf("%.8f ", max_error);
            max_error = 0;
            for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride : 0); i <= end[dims[0]]; i += stride) {
                for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride : 0); j <= end[dims[1]]; j += stride) {
                    for (size_t t = (begin[dims[3]] ? begin[dims[3]] + stride2x : 0);
                         t <= end[dims[3]]; t += stride2x) {
                        size_t begin_offset = i * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]] +
                                              begin[dims[2]] * dimension_offsets[dims[2]] +
                                              t * dimension_offsets[dims[3]];
                        predict_error += block_interpolation_1d(data, begin_offset,
                                                                begin_offset +
                                                                (end[dims[2]] - begin[dims[2]]) *
                                                                dimension_offsets[dims[2]],
                                                                stride * dimension_offsets[dims[2]], interp_func, pb,tuning);
                    }
                }
            }

//            printf("%.8f ", max_error);
            max_error = 0;
            for (size_t i = (begin[dims[0]] ? begin[dims[0]] + stride : 0); i <= end[dims[0]]; i += stride) {
                for (size_t j = (begin[dims[1]] ? begin[dims[1]] + stride : 0); j <= end[dims[1]]; j += stride) {
                    for (size_t k = (begin[dims[2]] ? begin[dims[2]] + stride : 0); k <= end[dims[2]]; k += stride) {
                        size_t begin_offset =
                                i * dimension_offsets[dims[0]] + j * dimension_offsets[dims[1]] +
                                k * dimension_offsets[dims[2]] +
                                begin[dims[3]] * dimension_offsets[dims[3]];
                        predict_error += block_interpolation_1d(data, begin_offset,
                                                                begin_offset +
                                                                (end[dims[3]] - begin[dims[3]]) *
                                                                dimension_offsets[dims[3]],
                                                                stride * dimension_offsets[dims[3]], interp_func, pb,tuning);
                    }
                }
            }
//            printf("%.8f \n", max_error);
            return predict_error;
        }
        bool anchor=false;
        int interpolation_level = -1;
        uint blocksize;
        int interpolator_id;
        double eb_ratio = 0.5;
        double alpha;
        double beta;
        std::vector<std::string> interpolators = {"linear", "cubic"};
        std::vector<int> quant_inds;
        //std::vector<bool> mark;
        size_t quant_index = 0; // for decompress
        size_t maxStep=0;
        double max_error;
        Quantizer quantizer;
        Encoder encoder;
        Lossless lossless;
        size_t num_elements;

        std::array<size_t, N> global_dimensions;
        std::array<size_t, N> dimension_offsets;
        std::vector<std::array<int, N>> dimension_sequences;
        int direction_sequence_id;

        std::vector<float> prediction_errors;//for test, to delete in final version
        int peTracking=0;//for test, to delete in final version

    };


};


#endif

