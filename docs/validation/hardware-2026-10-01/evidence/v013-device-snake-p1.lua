-- MosiCrokey game API v1. GLM SNAKE - 穿墙贪吃蛇
local snake, dx, dy, food, elapsed, score, dead, confirm_before
local function put_food()
    repeat
        food = {x = game.random(20) - 1, y = game.random(13) - 1}
        local occupied = false
        for _, p in ipairs(snake) do if p.x == food.x and p.y == food.y then occupied = true end end
        if not occupied then return end
    until false
end
function init()
    snake = {{x=8,y=6},{x=7,y=6},{x=6,y=6}}
    dx, dy, elapsed, score, dead, confirm_before = 1, 0, 0, 0, false, false
    food = {x=9, y=6}
end
function update(dt, input)
    if dead then
        if input.confirm and not confirm_before then init() end
        confirm_before = input.confirm
        return
    end
    if input.up and dy == 0 then dx,dy=0,-1
    elseif input.down and dy == 0 then dx,dy=0,1
    elseif input.left and dx == 0 then dx,dy=-1,0
    elseif input.right and dx == 0 then dx,dy=1,0 end
    if dx ~= 0 and dy ~= 0 then dx, dy = 0, dy end
    elapsed = elapsed + dt
    if elapsed < 0.25 then return end
    elapsed = elapsed - 0.25
    local head = {x=(snake[1].x+dx+20)%20, y=(snake[1].y+dy+13)%13}
    local eats = head.x==food.x and head.y==food.y
    for i=1,#snake-(eats and 0 or 1) do
        if snake[i].x==head.x and snake[i].y==head.y then dead=true return end
    end
    table.insert(snake,1,head)
    if eats then score=score+1; put_food()
    else table.remove(snake) end
end
function draw()
    game.clear(0x1083)
    game.text(4,4,"GLM SNAKE V2",0xffff)
    game.text(4,12,"SCORE "..score,0xffff)
    game.rect(0,20,160,104,0x0000)
    for i,p in ipairs(snake) do
        if i == 1 then
            game.rect(p.x*8+1,21+p.y*8,6,6,0xffff)
        else
            game.rect(p.x*8+1,21+p.y*8,6,6,0x07ff)
        end
    end
    game.rect(food.x*8+1,21+food.y*8,6,6,0xfca0)
    if dead then
        game.rect(28,48,104,28,0x2104)
        game.text(52,53,"GAME OVER",0xffff)
        game.text(34,65,"CONFIRM TO RESTART",0xfca0)
    end
end