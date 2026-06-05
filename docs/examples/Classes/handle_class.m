% Handle classes — BankAccount
% ============================
% A *handle* class (`classdef Name < handle`) has REFERENCE semantics:
% every variable that holds it points at the SAME underlying object.
% A method can mutate the object in place with no return value, and
% every alias sees the change. This is the opposite of a value class.
%
% BankAccount mutates its balance through deposit/withdraw and prints
% via a custom disp.

clear

classdef BankAccount < handle
  properties
    owner   = '?'
    balance = 0
  end
  methods
    function obj = BankAccount(owner, opening)
      obj.owner   = owner;
      obj.balance = opening;
    end

    % Mutates in place, returns nothing — pure handle-class style.
    function deposit(obj, amount)
      obj.balance = obj.balance + amount;
    end

    % Mutates AND returns whether it succeeded.
    function ok = withdraw(obj, amount)
      if amount > obj.balance
        ok = false;                      % insufficient funds
      else
        obj.balance = obj.balance - amount;
        ok = true;
      end
    end

    function disp(obj)
      fprintf('%-4s $%.2f\n', obj.owner, obj.balance);
    end
  end
end

acct = BankAccount('Ada', 100);
fprintf('opened             '); disp(acct);
acct.deposit(50);
fprintf('deposit(50)        '); disp(acct);
ok = acct.withdraw(30);
fprintf('withdraw(30) ok=%d  ', ok); disp(acct);
ok = acct.withdraw(1000);
fprintf('withdraw(1000) ok=%d ', ok); disp(acct);   % refused

% ── Reference semantics: an alias shares the SAME account ─────
same = acct;            % NOT a copy
same.deposit(1000);
fprintf('\nafter same.deposit(1000):\n');
fprintf('  same '); disp(same);
fprintf('  acct '); disp(acct);   % acct sees it too — they are one object
