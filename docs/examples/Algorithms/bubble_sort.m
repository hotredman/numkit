% Bubble Sort — Sort with swap counter
function result = bubbleSort(arr)
    n = length(arr);
    swaps = 0;
    for i = 1:n-1
        for j = 1:n-i
            if arr(j) > arr(j+1)
                temp = arr(j);
                arr(j) = arr(j+1);
                arr(j+1) = temp;
                swaps = swaps + 1;
            end
        end
    end
    result.sorted = arr;
    result.swaps  = swaps;
end

info = bubbleSort([64 34 25 12 22 11 90]);
disp(info.sorted)
